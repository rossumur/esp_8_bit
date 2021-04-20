// Reads PNG file and outputs 8-bit RGB332 pixel values
var fs = require("fs");
var PNG = require("pngjs").PNG;

fs.createReadStream("LoungingJubs.png")
  .pipe(
    new PNG({
      filterType:4,
    })
  )
  .on("parsed", function() {
    for (var y = 0; y < this.height; y++) {
      for (var x = 0; x < this.width; x++) {
        var idx = (y*this.width + x) << 2;
        var R = this.data[idx] & 0xE0;
        var G = (this.data[idx+1]>>5) << 2;
        var B = this.data[idx+2] >> 6;
        var RGB332 = R | G | B;

        console.log("0x"+Number(RGB332).toString(16)+", ");
      }
    }
  });
