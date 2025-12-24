// test_text_postprocessor.js
// 测试文本后处理器功能

// 引入模块（在浏览器环境中应该已经通过 script 标签加载）
// 这个文件可以在浏览器的控制台中运行

function runTests() {
  console.log('=== 开始测试文本后处理器 ===\n');
  
  // 测试1: 基本空行移除
  console.log('测试1: 基本空行移除');
  const test1Text = `第一行内容
  
第二行内容
   
第三行内容

第四行内容`;
  
  const processor1 = window.createTextPostProcessor({ removeBlankLines: true });
  const result1 = processor1.process(test1Text);
  
  console.log('原始文本:');
  console.log(JSON.stringify(test1Text));
  console.log('\n处理后文本:');
  console.log(JSON.stringify(result1.text));
  console.log('\n统计信息:', result1.stats);
  
  // 测试2: 字节偏移映射
  console.log('\n\n测试2: 字节偏移映射');
  const test2Text = `第一章

这是第一章的内容。

第二章

这是第二章的内容。`;
  
  const processor2 = window.createTextPostProcessor({ removeBlankLines: true });
  const result2 = processor2.process(test2Text);
  
  console.log('原始文本字节数:', new TextEncoder().encode(test2Text).length);
  console.log('处理后文本字节数:', new TextEncoder().encode(result2.text).length);
  
  // 模拟章节标记位置
  const chapterPositions = [0, 12, 42]; // "第一章", "第二章" 等的字节位置
  console.log('\n章节原始字节位置:', chapterPositions);
  
  const mappedPositions = chapterPositions.map(pos => ({
    original: pos,
    mapped: result2.byteOffsetMap(pos)
  }));
  console.log('映射后字节位置:', mappedPositions);
  
  // 验证映射正确性
  const encoder = new TextEncoder();
  console.log('\n验证映射正确性:');
  mappedPositions.forEach(({ original, mapped }) => {
    const originalSubstr = test2Text.slice(0, original);
    const mappedSubstr = result2.text.slice(0, mapped);
    console.log(`原始位置 ${original} → 映射位置 ${mapped}`);
    console.log(`  原始位置前的文本: "${originalSubstr.slice(-20)}"`);
    console.log(`  映射位置前的文本: "${mappedSubstr.slice(-20)}"`);
  });
  
  // 测试3: 混合空白字符
  console.log('\n\n测试3: 混合空白字符');
  const test3Text = `内容1
\t
内容2
   \t   
内容3
\r\n
内容4`;
  
  const processor3 = window.createTextPostProcessor({ removeBlankLines: true });
  const result3 = processor3.process(test3Text);
  
  console.log('原始文本行数:', test3Text.split('\n').length);
  console.log('处理后文本行数:', result3.text.split('\n').length);
  console.log('移除的空行数:', result3.stats.RemoveBlankLines.linesRemoved);
  
  // 测试4: 多语言文本
  console.log('\n\n测试4: 多语言文本（UTF-8字节计算）');
  const test4Text = `English text

中文文本

日本語テキスト

한국어 텍스트`;
  
  const processor4 = window.createTextPostProcessor({ removeBlankLines: true });
  const result4 = processor4.process(test4Text);
  
  const encoder4 = new TextEncoder();
  console.log('原始文本字节数:', encoder4.encode(test4Text).length);
  console.log('处理后文本字节数:', encoder4.encode(result4.text).length);
  console.log('减少的字节数:', result4.stats.RemoveBlankLines.bytesRemoved);
  
  // 测试5: 无空行文本
  console.log('\n\n测试5: 无空行文本（不应有变化）');
  const test5Text = `第一行
第二行
第三行`;
  
  const processor5 = window.createTextPostProcessor({ removeBlankLines: true });
  const result5 = processor5.process(test5Text);
  
  console.log('原始文本 === 处理后文本:', test5Text === result5.text);
  console.log('移除的空行数:', result5.stats.RemoveBlankLines.linesRemoved);
  
  // 测试6: 管道处理（为未来扩展准备）
  console.log('\n\n测试6: 处理器管道');
  const pipeline = new window.TextPostProcessorPipeline();
  pipeline.addProcessor(new window.RemoveBlankLinesProcessor());
  // 未来可以添加更多处理器：
  // pipeline.addProcessor(new NormalizeSpacesProcessor());
  // pipeline.addProcessor(new TrimLinesProcessor());
  
  const result6 = pipeline.process(test1Text);
  console.log('管道处理统计:', result6.stats);
  
  console.log('\n=== 所有测试完成 ===');
}

// 导出测试函数（如果在模块环境中）
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { runTests };
}

// 在浏览器控制台中运行：
// 1. 打开包含 text_postprocessor.js 的页面
// 2. 粘贴此文件内容到控制台
// 3. 运行: runTests()

console.log('测试函数已加载。运行 runTests() 开始测试。');
