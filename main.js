const rc522i2c = require('./build/Release/rc522-i2c');

rc522i2c.getSerial(17, 0x28, function (serial) {
	if(serial != ""){
		console.log(serial);
		process.exit(1)
	}
});
