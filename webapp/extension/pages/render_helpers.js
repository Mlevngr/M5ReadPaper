// render_helpers.js — 用于预览和 Demo 渲染的公共辅助函数
// 提供统一的位图解码接口，委托给各自的渲染器实现
// Version: 2025-01-26-v2

const RenderHelpers = {
  /**
   * 根据固件模式解码位图
   * 此函数作为统一入口，调用各自渲染器的解码方法
   * @param {Uint8Array} bmpBytes - 编码的位图字节
   * @param {number} bw - 位图宽度
   * @param {number} bh - 位图高度
   * @param {CanvasRenderingContext2D} ctx - Canvas 上下文
   * @param {string} mode - 固件模式（'readpaper'、'readpaper_v3' 或 'edc'）
   * @param {boolean} darkMode - 是否为暗黑模式（仅 V3 使用）
   * @returns {ImageData} 解码后的 ImageData
   */
  decodeBitmap(bmpBytes, bw, bh, ctx, mode, darkMode = false) {
    if (mode === 'edc') {
      // 使用 EDC 渲染器的解码方法
      if (typeof EdcRenderer !== 'undefined' && EdcRenderer.decodeToImageData) {
        return EdcRenderer.decodeToImageData(bmpBytes, bw, bh, ctx);
      }
      // 回退：如果渲染器未加载，尝试使用全局函数
      if (typeof window !== 'undefined' && window.EdcRenderer && window.EdcRenderer.decodeToImageData) {
        return window.EdcRenderer.decodeToImageData(bmpBytes, bw, bh, ctx);
      }
      throw new Error('EdcRenderer 未加载或不可用');
    } else if (mode === 'readpaper_v3') {
      // 使用 ReadPaper V3 渲染器的解码方法
      if (typeof ReadPaperV3Renderer !== 'undefined' && ReadPaperV3Renderer.decodeToImageData) {
        return ReadPaperV3Renderer.decodeToImageData(bmpBytes, bw, bh, ctx, darkMode);
      }
      // 回退：如果渲染器未加载，尝试使用全局函数
      if (typeof window !== 'undefined' && window.ReadPaperV3Renderer && window.ReadPaperV3Renderer.decodeToImageData) {
        return window.ReadPaperV3Renderer.decodeToImageData(bmpBytes, bw, bh, ctx, darkMode);
      }
      throw new Error('ReadPaperV3Renderer 未加载或不可用');
    } else {
      // 使用 ReadPaper 渲染器的解码方法
      if (typeof ReadPaperRenderer !== 'undefined' && ReadPaperRenderer.unpack1BitToImageData) {
        return ReadPaperRenderer.unpack1BitToImageData(bmpBytes, bw, bh, ctx);
      }
      // 回退：如果渲染器未加载，尝试使用全局函数
      if (typeof window !== 'undefined' && window.ReadPaperRenderer && window.ReadPaperRenderer.unpack1BitToImageData) {
        return window.ReadPaperRenderer.unpack1BitToImageData(bmpBytes, bw, bh, ctx);
      }
      throw new Error('ReadPaperRenderer 未加载或不可用');
    }
  }
};

// 导出为全局对象（浏览器和 Worker 环境）
if (typeof self !== 'undefined') {
  self.RenderHelpers = RenderHelpers;
}
if (typeof window !== 'undefined') {
  window.RenderHelpers = RenderHelpers;
}
