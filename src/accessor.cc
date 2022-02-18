#include <node.h>
#include <v8.h>
#include <unistd.h>
#include <uv.h>
#include <iostream>
#include "rfid.h"
#include "rc522.h"
#include "bcm2835.h"

using namespace std;

namespace rc522ic2
{
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Value;

struct Work
{

    Persistent<Function> callback;
    string result;
    uint8_t gpio_rst_pin;
    uint8_t i2c_address;
    bool stop;

    // rc522 vars
    char rfidChipSerialNumber[23];
    char rfidChipSerialNumberRecentlyDetected[23];
};

uint8_t serialNumber[10];
uint8_t serialNumberLength = 0;
char statusRfidReader;
uint16_t CType = 0;
int loopCounter;
uint8_t noTagFoundCount = 0;
char *p;

uint8_t initRfidReader(uint8_t pin_rst, uint8_t i2c_address)
{
    if (!bcm2835_init())
    {
        printf("Init Error\n");
        return 1;
    }

    bcm2835_i2c_begin();
    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_150);
    bcm2835_i2c_setSlaveAddress(i2c_address);
    bcm2835_i2c_set_baudrate(1000000); //1M baudrate
    return 0;
}

/**
  * WorkAsync function is the "middle" function which does the work.
  * After the WorkAsync function is called, the WorkAsyncComplete function
  * is called.
  */
static void WorkAsync(uv_work_t *req)
{
    Work *work = static_cast<Work *>(req->data);
    uint8_t res = initRfidReader(work->gpio_rst_pin, work->i2c_address);
    if (res == 1)
    {
        work->result = "Error";
        return;
    }
    InitRc522();
    bool running = true;
    // ButtonOn level of pin
    bool buttonOn = bcm2835_gpio_lev(work->gpio_rst_pin);
    // if Button = off result = "";
    if(!buttonOn){ work->result = string(""); }

    while (running && buttonOn)
    {
        //re check
        buttonOn = bcm2835_gpio_lev(work->gpio_rst_pin);
        statusRfidReader = find_tag(&CType);
        if (statusRfidReader == TAG_NOTAG)
        {
            // printf("NO_TAG_FOUND\n");
            // The status that no tag is found is sometimes set even when a tag is within reach of the tag reader
            // to prevent that the reset is performed the no tag event has to take place multiple times (ger: entrprellen)
            if (noTagFoundCount > 2)
            {
                // Sets the content of the array 'rfidChipSerialNumberRecentlyDetected' back to zero
                memset(&work->rfidChipSerialNumberRecentlyDetected[0], 0, sizeof(work->rfidChipSerialNumberRecentlyDetected));
                noTagFoundCount = 0;
            }
            else
            {
                noTagFoundCount++;
            }

            usleep(200000);
            continue;
        }
        else if (statusRfidReader != TAG_OK && statusRfidReader != TAG_COLLISION)
        {
            continue;
        }

        if (select_tag_sn(serialNumber, &serialNumberLength) != TAG_OK)
        {
            continue;
        }

        // Is a successful detected, the counter will be set to zero
        noTagFoundCount = 0;

        p = work->rfidChipSerialNumber;
        for (loopCounter = 0; loopCounter < serialNumberLength; loopCounter++)
        {
            sprintf(p, "%02x", serialNumber[loopCounter]);
            p += 2;
        }

        // Only when the serial number of the currently detected tag differs from the
        // recently detected tag the callback will be executed with the serial number
        if (strcmp(work->rfidChipSerialNumberRecentlyDetected, work->rfidChipSerialNumber) != 0)
        {
            // printf("TAG_FOUND:");
            work->result = string(work->rfidChipSerialNumber);
            running = false;
            usleep(1000000);

        }

        // Preserves the current detected serial number, so that it can be used
        // for future evaluations
        strcpy(work->rfidChipSerialNumberRecentlyDetected, work->rfidChipSerialNumber);

        *(p++) = 0;
    }

    bcm2835_i2c_end();
    bcm2835_close();
}

/**
  * WorkAsyncComplete function is called once we are ready to trigger the callback
  * function in JS.
  */
static void WorkAsyncComplete(uv_work_t *req, int status)
{
    Isolate *isolate = Isolate::GetCurrent();

    v8::HandleScope handleScope(isolate);

    Work *work = static_cast<Work *>(req->data);

    const char *result = work->result.c_str();
    Local<Value> argv[1] = {String::NewFromUtf8(isolate, result)};

    // https://stackoverflow.com/questions/13826803/calling-javascript-function-from-a-c-callback-in-v8/28554065#28554065
    Local<Function>::New(isolate, work->callback)->Call(isolate->GetCurrentContext()->Global(), 1, argv);

    if (work->stop)
    {
        work->callback.Reset();
        delete work;
    }
    else
    {
        uv_queue_work(uv_default_loop(), req, WorkAsync, WorkAsyncComplete);
    }
}

/**
  * getSerial is the initial function called from JS. This function returns
  * immediately, however starts a uv task which later calls the callback function
  */
void getSerial(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = args.GetIsolate();

    Work *work = new Work();

    Local<Function> callback = Local<Function>::Cast(args[2]);
    work->callback.Reset(isolate, callback);
    work->gpio_rst_pin = args[0]->IntegerValue();
    work->i2c_address = args[1]->IntegerValue();

    uv_work_t *req = new uv_work_t();
    req->data = work;

    uv_queue_work(uv_default_loop(), req, WorkAsync, WorkAsyncComplete);

    args.GetReturnValue().Set(Undefined(isolate));
}

/**
  * init function declares what we will make visible to node
  */
void initAsync(Local<Object> exports)
{
    NODE_SET_METHOD(exports, "getSerial", getSerial);
}

NODE_MODULE(rc522, initAsync)
}
