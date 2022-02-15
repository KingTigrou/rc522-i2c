import RPi.GPIO as GPIO         # import RPi.GPIO module
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)          # choose BCM or BOARD
GPIO.setup(17, GPIO.OUT)        # set a port/pin as an output
GPIO.output(17, 1)              # set port/pin value to 0/GPIO.LOW/False