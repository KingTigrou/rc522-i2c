const rc522i2c = require('./build/Release/rc522-i2c');

rc522i2c.getSerial(26, 0x28, function (serial) {
	console.log(serial);
});
