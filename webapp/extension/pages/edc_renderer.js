// edc_renderer.js — EDC 固件的字体渲染与打包逻辑
// 负责：灰度渲染 -> 灰度量化（4-bit）-> Huffman 编码打包
// Version: 2025-01-26-v1

/**
 * EDC 模式的渲染器
 * 使用灰度量化 + 可变长度编码（Huffman-like）
 */
const EdcRenderer = {
  /**
   * 处理裁剪后的图像数据，生成 EDC 格式的压缩位图
   * @param {ImageData} img - Canvas ImageData
   * @param {number} W - Canvas 宽度
   * @param {number} H - Canvas 高度
   * @param {number} minX - 内容区域左边界
   * @param {number} minY - 内容区域上边界
   * @param {number} maxX - 内容区域右边界
   * @param {number} maxY - 内容区域下边界
   * @param {number} whiteThreshold - 白色阈值（低于此值的灰度视为背景）
   * @param {number} blackThreshold - 黑色阈值（高于此值的灰度视为最黑）
   * @returns {Uint8Array} EDC 编码后的位图
   */
  processBitmap(img, W, H, minX, minY, maxX, maxY, whiteThreshold, blackThreshold) {
    const bw = maxX - minX + 1;
    const bh = maxY - minY + 1;
    const pixelCount = bw * bh;

    // 1. 提取并反转灰度图（EDC 使用反转灰度：0=白，255=黑）
    const grayscale = new Uint8Array(pixelCount);
    for (let yy = 0; yy < bh; yy++) {
      const srcRow = (minY + yy) * W;
      const dstRow = yy * bw;
      for (let xx = 0; xx < bw; xx++) {
        const idx = (srcRow + (minX + xx)) * 4;
        // 反转灰度：255 - R（使得黑色为 255，白色为 0）
        grayscale[dstRow + xx] = 255 - img.data[idx];
      }
    }

    // 2. 量化为 4-bit（0-15）
    const quantized = this._quantize4Bit(grayscale, whiteThreshold, blackThreshold);

    // 3. Huffman-like 编码
    return this._encodeHuffman(quantized);
  },

  /**
   * 将灰度图量化为 4-bit（0-15）
   * @param {Uint8Array} grayscale - 灰度图（0=白，255=黑）
   * @param {number} whiteThreshold - 白色阈值
   * @param {number} blackThreshold - 黑色阈值
   * @returns {Uint8Array} 量化后的 4-bit 值数组
   */
  _quantize4Bit(grayscale, whiteThreshold, blackThreshold) {
    const quantized = new Uint8Array(grayscale.length);
    for (let idx = 0; idx < grayscale.length; idx++) {
      const val = grayscale[idx];
      if (val < whiteThreshold) {
        // 背景/最白：量化为 15
        quantized[idx] = 15;
      } else if (val > blackThreshold) {
        // 最黑：量化为 0
        quantized[idx] = 0;
      } else {
        // 中间灰度：线性映射到 1-14
        let mid = Math.floor((blackThreshold - val) / 14);
        if (mid < 0) mid = 0;
        if (mid > 15) mid = 15;
        quantized[idx] = mid;
      }
    }
    return quantized;
  },

  /**
   * Huffman-like 编码：
   * - 值 15（最常见的背景）: 0 (1 bit)
   * - 值 0（纯黑）: 10 (2 bits)
   * - 值 1-14（中间灰度）: 11xxxx (6 bits, xxxx 为 4-bit 值)
   * @param {Uint8Array} quantized - 量化后的 4-bit 值数组
   * @returns {Uint8Array} 编码后的位流
   */
  _encodeHuffman(quantized) {
    const outBytes = [];
    let currentByte = 0;
    let bitPos = 7; // 从最高位开始

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

    for (let idx = 0; idx < quantized.length; idx++) {
      const q = quantized[idx];
      if (q === 15) {
        // 背景：0
        pushBit(0);
      } else if (q === 0) {
        // 纯黑：10
        pushBit(1);
        pushBit(0);
      } else {
        // 中间灰度 1-14：11xxxx
        pushBit(1);
        pushBit(1);
        // 写入 4-bit 值（高位先）
        for (let shift = 3; shift >= 0; shift--) {
          pushBit((q >> shift) & 1);
        }
      }
    }

    // 填充最后一个字节
    if (bitPos !== 7) {
      outBytes.push(currentByte & 0xFF);
    }

    return new Uint8Array(outBytes);
  },

  /**
   * 检测内容边界（用于裁剪）- EDC 模式
   * @param {Uint8Array} grayscale - 反转灰度图（0=白，255=黑）
   * @param {number} W - Canvas 宽度
   * @param {number} H - Canvas 高度
   * @param {number} whiteThreshold - 内容检测阈值（未使用，保留接口兼容）
   * @returns {object|null} {minX, minY, maxX, maxY} 或 null（无内容）
   */
  detectContentBounds(grayscale, W, H, whiteThreshold) {
    // EDC 使用反转灰度（0=白/背景，255=黑/前景）
    // 检测任何非零像素作为内容边界，确保抗锯齿边缘不被裁剪
    const contentDetect = 1; // 任何 > 0 的值都是内容
    let minY = H, maxY = -1, minX = W, maxX = -1;

    for (let yy = 0; yy < H; yy++) {
      const rowOffset = yy * W;
      for (let xx = 0; xx < W; xx++) {
        const val = grayscale[rowOffset + xx];
        if (val >= contentDetect) {
          if (yy < minY) minY = yy;
          if (yy > maxY) maxY = yy;
          if (xx < minX) minX = xx;
          if (xx > maxX) maxX = xx;
        }
      }
    }

    if (maxY >= minY && maxX >= minX) {
      return { minX, minY, maxX, maxY };
    }
    return null;
  },

  /**
   * 从 Canvas ImageData 创建完整的反转灰度图（用于边界检测）
   * @param {ImageData} img - Canvas ImageData
   * @param {number} W - Canvas 宽度
   * @param {number} H - Canvas 高度
   * @returns {Uint8Array} 反转灰度图
   */
  createGrayscale(img, W, H) {
    const grayscale = new Uint8Array(W * H);
    for (let idx = 0, p = 0; idx < grayscale.length; idx++, p += 4) {
      grayscale[idx] = 255 - img.data[p]; // 反转 R 通道
    }
    return grayscale;
  },

  /**
   * 解码 EDC 编码的位图为 ImageData（用于预览和 Demo 渲染）
   * @param {Uint8Array} bmpBytes - EDC 编码的位图字节
   * @param {number} bw - 位图宽度
   * @param {number} bh - 位图高度
   * @param {CanvasRenderingContext2D} ctx - Canvas 上下文（用于创建 ImageData）
   * @returns {ImageData} 解码后的灰度 ImageData
   */
  decodeToImageData(bmpBytes, bw, bh, ctx) {
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

    // 解码 Huffman-like 编码
    for (let i = 0; i < total; i++) {
      const first = readBit();
      if (first === 0) {
        values[i] = 15; // 背景/白色
        continue;
      }
      if (first === null) {
        values[i] = 15;
        continue;
      }
      const second = readBit();
      if (second === 0 || second === null) {
        values[i] = 0; // 纯黑
        continue;
      }
      // 读取 4-bit 值
      let val = 0;
      for (let b = 0; b < 4; b++) {
        const bit = readBit();
        val = (val << 1) | (bit === 1 ? 1 : 0);
      }
      values[i] = val & 0x0F;
    }

    // 将 4-bit 值（0-15）映射为灰度（0-255）
    const imgData = ctx.createImageData(bw, bh);
    const data = imgData.data;
    for (let i = 0; i < total; i++) {
      const v = values[i];
      // 0=最黑，15=最白，线性映射到 0-255
      const gray = Math.floor((v / 15) * 255);
      const idx = i * 4;
      data[idx] = gray;
      data[idx + 1] = gray;
      data[idx + 2] = gray;
      data[idx + 3] = 255;
    }

    return imgData;
  }
};

// 导出为全局对象（Worker 环境）
if (typeof self !== 'undefined') {
  self.EdcRenderer = EdcRenderer;
}

// 如果在浏览器主线程中，也暴露到 window，方便 RenderHelpers 调用
if (typeof window !== 'undefined') {
  window.EdcRenderer = EdcRenderer;
}
