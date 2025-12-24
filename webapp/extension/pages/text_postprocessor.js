// text_postprocessor.js
// 文本后处理模块 - 用于EPUB转TXT后的文本优化处理
// 采用策略模式设计，方便扩展各种文本处理器

/**
 * 文本处理器基类接口
 * 所有处理器都应该实现此接口
 */
class TextProcessor {
  /**
   * 处理文本
   * @param {string} text - 输入文本
   * @returns {{text: string, byteOffsetMap: Array<{oldOffset: number, newOffset: number}>}} 
   *          返回处理后的文本和字节偏移映射
   */
  process(text) {
    throw new Error('TextProcessor.process() must be implemented');
  }

  /**
   * 获取处理器名称
   * @returns {string}
   */
  getName() {
    throw new Error('TextProcessor.getName() must be implemented');
  }

  /**
   * 获取处理统计信息
   * @returns {Object}
   */
  getStats() {
    return {};
  }
}

/**
 * 空行移除处理器
 * 移除仅包含空白字符（空格、制表符、换行符等）的空行
 */
class RemoveBlankLinesProcessor extends TextProcessor {
  constructor() {
    super();
    this.stats = {
      linesRemoved: 0,
      bytesRemoved: 0
    };
  }

  getName() {
    return 'RemoveBlankLines';
  }

  /**
   * 处理文本，移除空行
   * @param {string} text - 输入文本
   * @returns {{text: string, byteOffsetMap: Array<{oldOffset: number, newOffset: number}>}}
   */
  process(text) {
    this.stats.linesRemoved = 0;
    this.stats.bytesRemoved = 0;

    const encoder = new TextEncoder();
    const lines = text.split('\n');
    const processedLines = [];
    const byteOffsetMap = [];
    
    let oldByteOffset = 0;  // 原始文本的字节偏移
    let newByteOffset = 0;  // 处理后文本的字节偏移
    
    // 记录每个关键位置的映射关系
    byteOffsetMap.push({ oldOffset: 0, newOffset: 0 });

    for (let i = 0; i < lines.length; i++) {
      const line = lines[i];
      const lineWithNewline = (i < lines.length - 1) ? line + '\n' : line;
      const lineBytes = encoder.encode(lineWithNewline).length;
      
      // 判断是否为空行（仅包含不可见字符）
      // 空行定义：只包含空格、制表符、回车等空白字符
      const isBlankLine = /^\s*$/.test(line);
      
      if (!isBlankLine) {
        // 保留非空行
        processedLines.push(line);
        
        // 记录这一行开始位置的映射
        byteOffsetMap.push({
          oldOffset: oldByteOffset + lineBytes,
          newOffset: newByteOffset + lineBytes
        });
        
        newByteOffset += lineBytes;
      } else {
        // 移除空行
        this.stats.linesRemoved++;
        this.stats.bytesRemoved += lineBytes;
        
        // 空行被删除，但仍需记录映射关系
        // 旧偏移继续增加，但新偏移保持不变（因为这些字节被删除了）
        byteOffsetMap.push({
          oldOffset: oldByteOffset + lineBytes,
          newOffset: newByteOffset
        });
      }
      
      oldByteOffset += lineBytes;
    }

    const processedText = processedLines.join('\n');
    
    return {
      text: processedText,
      byteOffsetMap: byteOffsetMap
    };
  }

  getStats() {
    return {
      linesRemoved: this.stats.linesRemoved,
      bytesRemoved: this.stats.bytesRemoved
    };
  }
}

/**
 * 文本后处理管道
 * 按顺序执行多个处理器，并维护字节偏移映射链
 */
class TextPostProcessorPipeline {
  constructor() {
    this.processors = [];
    this.stats = {};
  }

  /**
   * 添加处理器到管道
   * @param {TextProcessor} processor
   * @returns {TextPostProcessorPipeline} 返回自身以支持链式调用
   */
  addProcessor(processor) {
    if (!(processor instanceof TextProcessor)) {
      throw new Error('Processor must be an instance of TextProcessor');
    }
    this.processors.push(processor);
    return this;
  }

  /**
   * 执行所有处理器
   * @param {string} text - 输入文本
   * @returns {{text: string, byteOffsetMap: Function, stats: Object}}
   */
  process(text) {
    let currentText = text;
    let compositeMaps = [];
    this.stats = {};

    // 依次执行每个处理器
    for (const processor of this.processors) {
      const result = processor.process(currentText);
      currentText = result.text;
      compositeMaps.push(result.byteOffsetMap);
      
      // 收集统计信息
      const processorName = processor.getName();
      this.stats[processorName] = processor.getStats();
    }

    // 创建复合映射函数
    const compositeMapFunction = (oldOffset) => {
      return this._mapOffset(oldOffset, compositeMaps);
    };

    return {
      text: currentText,
      byteOffsetMap: compositeMapFunction,
      stats: this.stats
    };
  }

  /**
   * 将原始偏移通过多层映射链转换为最终偏移
   * @param {number} oldOffset - 原始字节偏移
   * @param {Array<Array<{oldOffset: number, newOffset: number}>>} maps - 映射链
   * @returns {number} 最终字节偏移
   * @private
   */
  _mapOffset(oldOffset, maps) {
    let currentOffset = oldOffset;
    
    // 依次通过每个映射表转换偏移
    for (const map of maps) {
      currentOffset = this._applySingleMap(currentOffset, map);
    }
    
    return currentOffset;
  }

  /**
   * 应用单个映射表
   * @param {number} offset - 输入偏移
   * @param {Array<{oldOffset: number, newOffset: number}>} map - 映射表
   * @returns {number} 输出偏移
   * @private
   */
  _applySingleMap(offset, map) {
    if (!map || map.length === 0) {
      return offset;
    }

    // 查找offset在映射表中的位置
    // 映射表按oldOffset排序
    for (let i = 0; i < map.length - 1; i++) {
      const current = map[i];
      const next = map[i + 1];
      
      if (offset >= current.oldOffset && offset < next.oldOffset) {
        // offset在current和next之间
        // 计算相对偏移并应用到新位置
        const relativeOffset = offset - current.oldOffset;
        return current.newOffset + relativeOffset;
      }
    }
    
    // 如果offset大于等于最后一个映射点
    const last = map[map.length - 1];
    if (offset >= last.oldOffset) {
      const relativeOffset = offset - last.oldOffset;
      return last.newOffset + relativeOffset;
    }
    
    // 如果offset小于第一个映射点（不应该发生）
    return offset;
  }

  /**
   * 获取处理统计信息
   * @returns {Object}
   */
  getStats() {
    return this.stats;
  }
}

/**
 * 工厂函数：根据配置创建处理管道
 * @param {Object} options - 处理选项
 * @param {boolean} options.removeBlankLines - 是否移除空行
 * @returns {TextPostProcessorPipeline}
 */
function createTextPostProcessor(options = {}) {
  const pipeline = new TextPostProcessorPipeline();
  
  // 根据选项添加处理器
  if (options.removeBlankLines) {
    pipeline.addProcessor(new RemoveBlankLinesProcessor());
  }
  
  // 未来可以在这里添加更多处理器
  // if (options.normalizeSpaces) {
  //   pipeline.addProcessor(new NormalizeSpacesProcessor());
  // }
  // if (options.removeExtraLineBreaks) {
  //   pipeline.addProcessor(new RemoveExtraLineBreaksProcessor());
  // }
  
  return pipeline;
}

// 导出
if (typeof module !== 'undefined' && module.exports) {
  // Node.js 环境
  module.exports = {
    TextProcessor,
    RemoveBlankLinesProcessor,
    TextPostProcessorPipeline,
    createTextPostProcessor
  };
}

// 浏览器环境 - 添加到全局作用域
if (typeof window !== 'undefined') {
  window.TextProcessor = TextProcessor;
  window.RemoveBlankLinesProcessor = RemoveBlankLinesProcessor;
  window.TextPostProcessorPipeline = TextPostProcessorPipeline;
  window.createTextPostProcessor = createTextPostProcessor;
}
