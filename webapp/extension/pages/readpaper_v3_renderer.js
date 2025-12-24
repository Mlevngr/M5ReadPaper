// readpaper_v3_renderer.js — ReadPaper V3 固件的字体渲染与打包逻辑
// 负责：灰度渲染 -> 三色量化（白/灰/黑）-> 2-bit Huffman 编码
// Version: 2025-01-27-v3

/**
 * ReadPaper V3 模式的渲染器
 * 使用三色量化 + 2-bit Huffman 编码：
 * - 0 (1 bit) = 白色（背景）
 * - 10 (2 bits) = 灰色（中间色）
 * - 11 (2 bits) = 黑色（前景）
 */
const ReadPaperV3Renderer = {
  version: "2025-01-27-v3",

  /**
   * 处理裁剪后的图像数据，生成 V3 格式的 2-bit Huffman 编码位图
   * @param {ImageData} img - Canvas ImageData
   * @param {number} W - Canvas 宽度
   * @param {number} H - Canvas 高度
   * @param {number} minX - 内容区域左边界
   * @param {number} minY - 内容区域上边界
   * @param {number} maxX - 内容区域右边界
   * @param {number} maxY - 内容区域下边界
   * @param {number} whiteThreshold - 白色阈值（高于此值为白色背景）
   * @param {number} grayThreshold - 灰色阈值（介于灰色和黑色之间）
   * @param {boolean} enableDenoise - 是否启用去噪
   * @param {boolean} enableGaussian - 是否启用高斯模糊
   * @param {number} gaussianRadius - 高斯模糊半径
   * @param {number} gaussianSigma - 高斯模糊标准差
   * @param {boolean} enableBilateral - 是否启用双边滤波
   * @param {number} bilateralRadius - 双边滤波半径
   * @param {number} bilateralSigmaSpace - 双边滤波空间标准差
   * @param {number} bilateralSigmaRange - 双边滤波范围标准差
   * @returns {Uint8Array} V3 编码后的位图
   */
  processBitmap(
    img, W, H, minX, minY, maxX, maxY,
    whiteThreshold, grayThreshold,
    enableDenoise,
    enableGaussian, gaussianRadius, gaussianSigma,
    enableBilateral, bilateralRadius, bilateralSigmaSpace, bilateralSigmaRange
  ) {
    const bw = maxX - minX + 1;
    const bh = maxY - minY + 1;

    // 1. 提取灰度图（裁剪到内容区域）
    const gray = new Uint8Array(bw * bh);
    for (let yy = 0; yy < bh; yy++) {
      for (let xx = 0; xx < bw; xx++) {
        const sidx = ((minY + yy) * W + (minX + xx)) * 4;
        gray[yy * bw + xx] = img.data[sidx]; // R 通道作为灰度
      }
    }

    // 2. 可选：预处理滤波（减少噪点和抗锯齿残留）
    if (enableGaussian) {
      try {
        this._gaussianBlurGray(gray, bw, bh,
          (typeof gaussianRadius === 'number' ? gaussianRadius : 1),
          (typeof gaussianSigma === 'number' ? gaussianSigma : 1.0)
        );
      } catch (e) { /* 忍受失败 */ }
    }
    if (enableBilateral) {
      try {
        this._bilateralFilterGray(gray, bw, bh,
          (typeof bilateralRadius === 'number' ? bilateralRadius : 2),
          (typeof bilateralSigmaSpace === 'number' ? bilateralSigmaSpace : 2),
          (typeof bilateralSigmaRange === 'number' ? bilateralSigmaRange : 25)
        );
      } catch (e) { /* 忍受失败 */ }
    }

    // 3. 三色量化：白(0) / 灰(1) / 黑(2)
    const tri = new Uint8Array(bw * bh);
    for (let i = 0; i < bw * bh; i++) {
      const g = gray[i];
      if (g >= whiteThreshold) {
        tri[i] = 0; // 白色
      } else if (g >= grayThreshold) {
        tri[i] = 1; // 灰色
      } else {
        tri[i] = 2; // 黑色
      }
    }

    // 4. 可选：三色域去噪（移除孤立噪点，填补小空洞）
    if (enableDenoise) {
      this._denoiseTricolor(tri, bw, bh);
    }

    // 5. 2-bit Huffman 编码：0 -> '0', 灰 -> '10', 黑 -> '11'
    return this._encode2BitHuffman(tri, bw, bh);
  },

  /**
   * 高斯模糊（与 ReadPaper V2 相同）
   */
  _gaussianBlurGray(gray, bw, bh, radius, sigma) {
    radius = Math.max(0, Math.min(10, Math.floor(radius) || 1));
    sigma = (typeof sigma === 'number' && sigma > 0) ? sigma : (radius > 0 ? radius : 1.0);
    const ksize = radius * 2 + 1;
    const kernel = new Float32Array(ksize);
    const twoSigma2 = 2 * sigma * sigma;
    let ksum = 0;
    for (let i = -radius; i <= radius; i++) {
      const v = Math.exp(-(i * i) / twoSigma2);
      kernel[i + radius] = v;
      ksum += v;
    }
    for (let i = 0; i < ksize; i++) kernel[i] /= ksum;

    const tmp = new Float32Array(bw * bh);

    // 水平卷积
    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        let acc = 0;
        for (let k = -radius; k <= radius; k++) {
          const xx = x + k;
          if (xx < 0 || xx >= bw) continue;
          acc += gray[y * bw + xx] * kernel[k + radius];
        }
        tmp[y * bw + x] = acc;
      }
    }

    // 垂直卷积
    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        let acc = 0;
        for (let k = -radius; k <= radius; k++) {
          const yy = y + k;
          if (yy < 0 || yy >= bh) continue;
          acc += tmp[yy * bw + x] * kernel[k + radius];
        }
        gray[y * bw + x] = Math.max(0, Math.min(255, Math.round(acc)));
      }
    }
  },

  /**
   * 双边滤波（与 ReadPaper V2 相同）
   */
  _bilateralFilterGray(gray, bw, bh, radius, sigmaSpace, sigmaRange) {
    radius = Math.max(0, Math.min(8, Math.floor(radius) || 1));
    sigmaSpace = (typeof sigmaSpace === 'number' && sigmaSpace > 0) ? sigmaSpace : Math.max(1.0, radius);
    sigmaRange = (typeof sigmaRange === 'number' && sigmaRange > 0) ? sigmaRange : 25;

    const twoSpace2 = 2 * sigmaSpace * sigmaSpace;
    const twoRange2 = 2 * sigmaRange * sigmaRange;
    const spatial = new Float32Array((radius * 2 + 1) * (radius * 2 + 1));
    let idx = 0;
    for (let oy = -radius; oy <= radius; oy++) {
      for (let ox = -radius; ox <= radius; ox++) {
        spatial[idx++] = Math.exp(-(ox * ox + oy * oy) / twoSpace2);
      }
    }

    const out = new Uint8Array(bw * bh);
    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        const center = gray[y * bw + x];
        let wsum = 0;
        let acc = 0;
        idx = 0;
        for (let oy = -radius; oy <= radius; oy++) {
          const yy = y + oy;
          if (yy < 0 || yy >= bh) { idx += (radius * 2 + 1); continue; }
          for (let ox = -radius; ox <= radius; ox++) {
            const xx = x + ox;
            if (xx < 0 || xx >= bw) { idx++; continue; }
            const val = gray[yy * bw + xx];
            const rangeW = Math.exp(-((val - center) * (val - center)) / twoRange2);
            const w = spatial[idx] * rangeW;
            acc += val * w;
            wsum += w;
            idx++;
          }
        }
        out[y * bw + x] = wsum > 0 ? Math.round(acc / wsum) : center;
      }
    }
    for (let i = 0; i < bw * bh; i++) gray[i] = out[i];
  },

  /**
   * 三色域去噪：移除孤立噪点，填补小空洞
   * @param {Uint8Array} tri - 三色图（0=白，1=灰，2=黑）
   * @param {number} bw - 宽度
   * @param {number} bh - 高度
   */
  _denoiseTricolor(tri, bw, bh) {
    const out = new Uint8Array(bw * bh);
    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        const self = tri[y * bw + x];
        
        // 统计 3x3 邻域中各颜色的数量
        const count = [0, 0, 0]; // [白, 灰, 黑]
        for (let oy = -1; oy <= 1; oy++) {
          const yy = y + oy;
          if (yy < 0 || yy >= bh) continue;
          for (let ox = -1; ox <= 1; ox++) {
            const xx = x + ox;
            if (xx < 0 || xx >= bw) continue;
            count[tri[yy * bw + xx]]++;
          }
        }

        // 去噪策略：如果当前像素是孤立的（邻域中同色少于2个），使用邻域最多的颜色
        if (count[self] < 2) {
          // 找出邻域中最多的颜色
          let maxCount = count[0];
          let maxColor = 0;
          for (let c = 1; c < 3; c++) {
            if (count[c] > maxCount) {
              maxCount = count[c];
              maxColor = c;
            }
          }
          out[y * bw + x] = maxColor;
        } else {
          out[y * bw + x] = self;
        }
      }
    }
    // 写回
    for (let i = 0; i < bw * bh; i++) tri[i] = out[i];
  },

  /**
   * 2-bit Huffman 编码：
   * - 白色(0) -> '0' (1 bit)
   * - 灰色(1) -> '10' (2 bits)
   * - 黑色(2) -> '11' (2 bits)
   * @param {Uint8Array} tri - 三色图（0=白，1=灰，2=黑）
   * @param {number} bw - 宽度
   * @param {number} bh - 高度
   * @returns {Uint8Array} 编码后的位流
   */
  _encode2BitHuffman(tri, bw, bh) {
    const outBytes = [];
    let currentByte = 0;
    let bitPos = 7; // MSB first

    const pushBit = (bitValue) => {
      if (bitValue) {
        currentByte |= (1 << bitPos);
      }
      bitPos--;
      if (bitPos < 0) {
        outBytes.push(currentByte & 0xFF);
        currentByte = 0;
        bitPos = 7;
      }
    };

    for (let i = 0; i < bw * bh; i++) {
      const color = tri[i];
      if (color === 0) {
        // 白色：0
        pushBit(0);
      } else if (color === 1) {
        // 灰色：10
        pushBit(1);
        pushBit(0);
      } else {
        // 黑色：11
        pushBit(1);
        pushBit(1);
      }
    }

    // 填充最后一个字节
    if (bitPos !== 7) {
      outBytes.push(currentByte & 0xFF);
    }

    return new Uint8Array(outBytes);
  },

  /**
   * 解码 V3 编码的位图为三色数组（用于预览）
   * @param {Uint8Array} bmpBytes - V3 编码的位图字节
   * @param {number} bw - 位图宽度
   * @param {number} bh - 位图高度
   * @returns {Uint8Array} 三色数组（0=白，1=灰，2=黑）
   */
  decodeV3Bitmap(bmpBytes, bw, bh) {
    const bytes = bmpBytes instanceof Uint8Array ? bmpBytes : new Uint8Array(bmpBytes || []);
    const total = bw * bh;
    const values = new Uint8Array(total);
    let byteIndex = 0;
    let bitIndex = 7;

    const readBit = () => {
      if (byteIndex >= bytes.length) return null;
      const bit = (bytes[byteIndex] >> bitIndex) & 1;
      bitIndex--;
      if (bitIndex < 0) {
        bitIndex = 7;
        byteIndex++;
      }
      return bit;
    };

    // 解码 2-bit Huffman
    for (let i = 0; i < total; i++) {
      const first = readBit();
      if (first === 0) {
        // 白色
        values[i] = 0;
        continue;
      }
      if (first === null) {
        values[i] = 0; // 默认背景
        continue;
      }
      // first === 1，读取第二个 bit
      const second = readBit();
      if (second === 0) {
        // 10 -> 灰色
        values[i] = 1;
      } else {
        // 11 -> 黑色
        values[i] = 2;
      }
    }

    return values;
  },

  /**
   * 将三色数组转换为 ImageData（用于预览渲染）
   * @param {Uint8Array} triColor - 三色数组（0=白，1=灰，2=黑）
   * @param {number} bw - 位图宽度
   * @param {number} bh - 位图高度
   * @param {CanvasRenderingContext2D} ctx - Canvas 上下文
   * @param {boolean} darkMode - 是否为暗黑模式
   * @returns {ImageData} 渲染后的 ImageData
   */
  triColorToImageData(triColor, bw, bh, ctx, darkMode = false) {
    const imgData = ctx.createImageData(bw, bh);
    const data = imgData.data;

    // 预览用颜色：灰色使用 0x888888 -> RGB(136,136,136)
    const colors = darkMode ? {
      white: [0, 0, 0],        // 暗黑模式：黑色背景
      gray: [136, 136, 136],   // 0x888888 -> RGB(136,136,136)
      black: [255, 255, 255]   // 暗黑模式：白色前景
    } : {
      white: [255, 255, 255],  // 白色背景
      gray: [136, 136, 136],   // 0x888888 -> RGB(136,136,136)
      black: [0, 0, 0]         // 黑色前景
    };

    for (let i = 0; i < triColor.length; i++) {
      const color = triColor[i];
      let rgb;
      if (color === 0) {
        rgb = colors.white;
      } else if (color === 1) {
        rgb = colors.gray;
      } else {
        rgb = colors.black;
      }

      const idx = i * 4;
      data[idx] = rgb[0];
      data[idx + 1] = rgb[1];
      data[idx + 2] = rgb[2];
      data[idx + 3] = 255;
    }

    return imgData;
  },

  /**
   * 解码 V3 位图并转换为 ImageData（组合方法，用于预览）
   * @param {Uint8Array} bmpBytes - V3 编码的位图字节
   * @param {number} bw - 位图宽度
   * @param {number} bh - 位图高度
   * @param {CanvasRenderingContext2D} ctx - Canvas 上下文
   * @param {boolean} darkMode - 是否为暗黑模式
   * @returns {ImageData} 解码并渲染后的 ImageData
   */
  decodeToImageData(bmpBytes, bw, bh, ctx, darkMode = false) {
    const triColor = this.decodeV3Bitmap(bmpBytes, bw, bh);
    return this.triColorToImageData(triColor, bw, bh, ctx, darkMode);
  }
};

// 导出为全局对象（Worker 环境）
if (typeof self !== 'undefined') {
  self.ReadPaperV3Renderer = ReadPaperV3Renderer;
}

// 浏览器主线程也暴露到 window
if (typeof window !== 'undefined') {
  window.ReadPaperV3Renderer = ReadPaperV3Renderer;
}

// 自检方法：在页面控制台中调用 ReadPaperV3Renderer.selfTest() 来验证编码/解码对称性
if (typeof window !== 'undefined') {
  window.ReadPaperV3Renderer.selfTest = function() {
    try {
      const test = new Uint8Array([0,0,0,1,1,1,2,2,2]); // 3x3 test pattern
      const encoded = ReadPaperV3Renderer._encode2BitHuffman(test, 3, 3);
      const decoded = ReadPaperV3Renderer.decodeV3Bitmap(encoded, 3, 3);
      const match = test.length === decoded.length && test.every((v, i) => v === decoded[i]);
      console.log('ReadPaperV3Renderer.selfTest -> match:', match);
      console.log('Encoded bytes:', Array.from(encoded).map(b => b.toString(2).padStart(8, '0')).join(' '));
      console.log('Original:', Array.from(test));
      console.log('Decoded :', Array.from(decoded));
      return { match, encoded, original: test, decoded };
    } catch (e) {
      console.error('selfTest failed:', e);
      return { match: false, error: (e && e.message) ? e.message : String(e) };
    }
  };
}
