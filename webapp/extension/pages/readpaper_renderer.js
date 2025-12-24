// readpaper_renderer.js — ReadPaper 固件的字体渲染与打包逻辑
// 负责：灰度渲染 -> 阈值二值化 -> 可选平滑 -> 1-bit 打包
// Version: 2025-01-26-v1

/**
 * ReadPaper 模式的渲染器
 * 使用单一阈值二值化 + 可选的二值域平滑
 */
const ReadPaperRenderer = {
  version: "2025-01-27-v2-tuned",
  /**
   * 处理裁剪后的图像数据，生成 1-bit 打包位图
   * @param {ImageData} img - Canvas ImageData
   * @param {number} W - Canvas 宽度
   * @param {number} H - Canvas 高度
   * @param {number} minX - 内容区域左边界
   * @param {number} minY - 内容区域上边界
   * @param {number} maxX - 内容区域右边界
   * @param {number} maxY - 内容区域下边界
   * @param {number} whiteThreshold - 白色阈值（小于此值为黑色）
   * @param {number} smoothingPasses - 平滑次数（0 表示不平滑）
   * @param {number} strokeExpand - 笔画加粗（0-3）
   * @param {boolean} enableDenoise - 是否启用连续邻域去噪
   * @returns {Uint8Array} 打包后的 1-bit 位图
   */
  processBitmap(img, W, H, minX, minY, maxX, maxY, whiteThreshold, smoothingPasses, strokeExpand, enableDenoise, enableGaussian, gaussianRadius, gaussianSigma, enableBilateral, bilateralRadius, bilateralSigmaSpace, bilateralSigmaRange) {
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

    // 可选：先做平滑（高斯或双边）以减少椒盐或抗锯齿残留
    if (enableGaussian) {
      try {
        this._gaussianBlurGray(gray, bw, bh, (typeof gaussianRadius === 'number' ? gaussianRadius : 1), (typeof gaussianSigma === 'number' ? gaussianSigma : 1.0));
      } catch (e) { /* 忍受失败，回退到原始灰度 */ }
    }
    if (enableBilateral) {
      try {
        this._bilateralFilterGray(gray, bw, bh, (typeof bilateralRadius === 'number' ? bilateralRadius : 2), (typeof bilateralSigmaSpace === 'number' ? bilateralSigmaSpace : 2), (typeof bilateralSigmaRange === 'number' ? bilateralSigmaRange : 25));
      } catch (e) { /* 忍受失败，回退到原始灰度 */ }
    }

    // 2. 二值化：使用固定阈值
    const bin = new Uint8Array(bw * bh);
    for (let i = 0; i < bw * bh; i++) {
      bin[i] = (gray[i] < whiteThreshold) ? 1 : 0;
    }

    // 2.5 连续邻域去噪（Otsu 后处理）
    if (enableDenoise) {
      // Step 1: 去除噪点（Opening-like）- 先消除孤立黑点，防止它们干扰填补判断
      // Step 2: 填补空洞（Closing-like）- 仅修补极小的封闭空洞
      const cleaned = this._removeNoise(bin, bw, bh);
      const filled = this._fillHoles(cleaned, bw, bh);
      bin.set(filled);
    }

    // 3. 可选的二值域平滑（减少锯齿和噪点）
    const applySmoothing = (smoothingPasses && smoothingPasses > 0);
    if (applySmoothing) {
      let cur = bin;
      for (let sp = 0; sp < smoothingPasses; sp++) {
        cur = this._safePass(cur, bw, bh);
      }
      bin.set(cur);
    }

    // 3.5 可选笔画加粗（膨胀），用于补偿过细或断笔的情况
    strokeExpand = (typeof strokeExpand === 'number') ? Math.max(0, Math.min(3, Math.floor(strokeExpand))) : 0;
    if (strokeExpand > 0) {
      let cur = bin;
      for (let p = 0; p < strokeExpand; p++) {
        cur = this._dilate(cur, bw, bh);
      }
      bin.set(cur);
    }

    // 4. 打包为 1-bit 位图（MSB first）
    return this._pack1Bit(bin, bw, bh);
  },

  /**
   * 对灰度图做可分离的高斯模糊（原地修改 gray）
   * @param {Uint8Array} gray
   * @param {number} bw
   * @param {number} bh
   * @param {number} radius
   * @param {number} sigma
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

    // 临时缓冲
    const tmp = new Float32Array(bw * bh);

    // 水平卷积
    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        let acc = 0;
        for (let k = -radius; k <= radius; k++) {
          const xx = x + k;
          if (xx < 0) continue;
          if (xx >= bw) continue;
          acc += gray[y * bw + xx] * kernel[k + radius];
        }
        tmp[y * bw + x] = acc;
      }
    }

    // 垂直卷积并写回 gray
    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        let acc = 0;
        for (let k = -radius; k <= radius; k++) {
          const yy = y + k;
          if (yy < 0) continue;
          if (yy >= bh) continue;
          acc += tmp[yy * bw + x] * kernel[k + radius];
        }
        gray[y * bw + x] = Math.max(0, Math.min(255, Math.round(acc)));
      }
    }
  },

  /**
   * 简单的双边滤波（在小半径上进行），原地修改 gray
   * radius: 空间半径（像素），sigmaSpace: 空间高斯方差近似，sigmaRange: 灰度范围方差
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
    // 写回
    for (let i = 0; i < bw * bh; i++) gray[i] = out[i];
  },

  /**
   * 安全平滑：去噪 + 填洞（保守策略，避免断笔或合并笔画）
   * @param {Uint8Array} src - 源二值图（1=黑，0=白）
   * @param {number} bw - 宽度
   * @param {number} bh - 高度
   * @returns {Uint8Array} 平滑后的二值图
   */
  _safePass(src, bw, bh) {
    const out = new Uint8Array(bw * bh);
    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        let sum = 0;
        // 统计 3x3 邻域（包括自身）
        for (let oy = -1; oy <= 1; oy++) {
          const yy = y + oy;
          if (yy < 0 || yy >= bh) continue;
          for (let ox = -1; ox <= 1; ox++) {
            const xx = x + ox;
            if (xx < 0 || xx >= bw) continue;
            sum += src[yy * bw + xx];
          }
        }

        const self = src[y * bw + x];
        if (self === 1) {
          // 黑色像素：保留非孤立的（至少有 1 个邻居 + 自己 = sum >= 2）
          // 移除单像素噪点（sum = 1）
          out[y * bw + x] = (sum >= 2) ? 1 : 0;
        } else {
          // 白色像素：填充被包围的（至少 7 个邻居为黑 = sum >= 7）
          // 填补单像素洞但保留 1px 间隙（sum = 6）
          out[y * bw + x] = (sum >= 7) ? 1 : 0;
        }
      }
    }
    return out;
  },

  /**
   * 打包二值图为 1-bit 位图（MSB first，与 Python 版本一致）
   * @param {Uint8Array} bin - 二值图（1=黑，0=白）
   * @param {number} bw - 宽度
   * @param {number} bh - 高度
   * @returns {Uint8Array} 打包后的位图
   */
  _pack1Bit(bin, bw, bh) {
    const bytesPerRow = Math.ceil(bw / 8);
    const bmp = new Uint8Array(bytesPerRow * bh);

    for (let yy = 0; yy < bh; yy++) {
      for (let bx = 0; bx < bytesPerRow; bx++) {
        let byteVal = 0;
        for (let bit = 0; bit < 8; bit++) {
          const xbit = bx * 8 + bit;
          if (xbit >= bw) break;
          const bitVal = bin[yy * bw + xbit];
          if (bitVal) {
            byteVal |= (1 << (7 - bit));
          }
        }
        bmp[yy * bytesPerRow + bx] = byteVal;
      }
    }

    // 对每行最后一个字节取反并应用掩码（与 Python 版本保持一致）
    if (bmp.length > 0) {
      const lastBits = bw % 8;
      const lastMask = lastBits === 0 ? 0xFF : (((1 << lastBits) - 1) << (8 - lastBits)) & 0xFF;
      for (let yy = 0; yy < bh; yy++) {
        const rowStart = yy * bytesPerRow;
        for (let bx = 0; bx < bytesPerRow; bx++) {
          const mask = (bx === bytesPerRow - 1) ? lastMask : 0xFF;
          bmp[rowStart + bx] = (~bmp[rowStart + bx]) & mask;
        }
      }
    }

    return bmp;
  },

  /**
   * 检测内容边界（用于裁剪）
   * @param {ImageData} img - Canvas ImageData
   * @param {number} W - Canvas 宽度
   * @param {number} H - Canvas 高度
   * @param {number} whiteThreshold - 内容检测阈值
   * @returns {object|null} {minX, minY, maxX, maxY} 或 null（无内容）
   */
  detectContentBounds(img, W, H, whiteThreshold) {
    const contentDetect = 255 - whiteThreshold;
    let minY = H, maxY = -1, minX = W, maxX = -1;

    for (let yy = 0; yy < H; yy++) {
      for (let xx = 0; xx < W; xx++) {
        const idx = (yy * W + xx) * 4;
        const r = img.data[idx];
        if (r < contentDetect) {
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
   * 解包 1-bit 位图为 ImageData（用于预览和 Demo 渲染）
   * @param {Uint8Array} bmpBytes - 打包的位图字节
   * @param {number} bw - 位图宽度
   * @param {number} bh - 位图高度
   * @param {CanvasRenderingContext2D} ctx - Canvas 上下文（用于创建 ImageData）
   * @returns {ImageData} 解包后的 ImageData（黑白图像）
   */
  unpack1BitToImageData(bmpBytes, bw, bh, ctx) {
    const bytesPerRow = Math.ceil(bw / 8);
    const view = bmpBytes instanceof Uint8Array ? bmpBytes : new Uint8Array(bmpBytes || []);
    const imgData = ctx.createImageData(bw, bh);
    const data = imgData.data;

    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        const byte = view[y * bytesPerRow + (x >> 3)];
        const bit = 7 - (x & 7);
        const isInk = (byte & (1 << bit)) !== 0;
        // 反转显示极性：bit=1 为白色（255），bit=0 为黑色（0）
        const col = isInk ? 255 : 0;
        const idx = (y * bw + x) * 4;
        data[idx] = col;
        data[idx + 1] = col;
        data[idx + 2] = col;
        data[idx + 3] = 255;
      }
    }

    return imgData;
  }

  ,

  /**
   * 简单膨胀（3x3） — 将黑色像素扩展到邻域以加粗笔画
   * @param {Uint8Array} src - 二值图（1=黑）
   * @param {number} bw
   * @param {number} bh
   * @returns {Uint8Array}
   */
  _dilate(src, bw, bh) {
    const out = new Uint8Array(bw * bh);
    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        let v = 0;
        for (let oy = -1; oy <= 1; oy++) {
          const yy = y + oy;
          if (yy < 0 || yy >= bh) continue;
          for (let ox = -1; ox <= 1; ox++) {
            const xx = x + ox;
            if (xx < 0 || xx >= bw) continue;
            if (src[yy * bw + xx]) { v = 1; break; }
          }
          if (v) break;
        }
        out[y * bw + x] = v;
      }
    }
    return out;
  },

  /**
   * 填补空洞（Closing-like）：
   * 对每个白色像素，如果其 8 邻域中黑色像素的数量 >= 7，则将其填充为黑色。
   * 用于修补笔画边缘的凹槽和内部微小空洞。
   */
  _fillHoles(src, bw, bh) {
    const out = new Uint8Array(bw * bh);
    out.set(src);

    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        if (src[y * bw + x] === 0) { // White
          let blackNeighbors = 0;
          for (let oy = -1; oy <= 1; oy++) {
            const yy = y + oy;
            if (yy < 0 || yy >= bh) continue;
            for (let ox = -1; ox <= 1; ox++) {
              if (ox === 0 && oy === 0) continue;
              const xx = x + ox;
              if (xx < 0 || xx >= bw) continue;
              if (src[yy * bw + xx] === 1) blackNeighbors++;
            }
          }
          // Threshold: >= 7 black neighbors -> fill
          if (blackNeighbors >= 7) {
            out[y * bw + x] = 1;
          }
        }
      }
    }
    return out;
  },

  /**
   * 去除噪点（Opening-like）：
   * 对每个黑色像素，如果其 8 邻域中黑色像素的数量 <= 2，则将其移除（变白）。
   * 用于消除孤立的散佚噪点。
   */
  _removeNoise(src, bw, bh) {
    const out = new Uint8Array(bw * bh);
    out.set(src);

    for (let y = 0; y < bh; y++) {
      for (let x = 0; x < bw; x++) {
        if (src[y * bw + x] === 1) { // Black
          let blackNeighbors = 0;
          for (let oy = -1; oy <= 1; oy++) {
            const yy = y + oy;
            if (yy < 0 || yy >= bh) continue;
            for (let ox = -1; ox <= 1; ox++) {
              if (ox === 0 && oy === 0) continue;
              const xx = x + ox;
              if (xx < 0 || xx >= bw) continue;
              if (src[yy * bw + xx] === 1) blackNeighbors++;
            }
          }
          // Threshold: <= 2 black neighbors -> remove (noise)
          if (blackNeighbors <= 2) {
            out[y * bw + x] = 0;
          }
        }
      }
    }
    return out;
  },
};

// 导出为全局对象（Worker 环境）
if (typeof self !== 'undefined') {
  self.ReadPaperRenderer = ReadPaperRenderer;
}

// 如果在浏览器主线程中，也暴露到 window，方便 RenderHelpers 调用
if (typeof window !== 'undefined') {
  window.ReadPaperRenderer = ReadPaperRenderer;
}
