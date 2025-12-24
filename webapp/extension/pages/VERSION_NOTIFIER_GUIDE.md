# 版本更新通知功能

## 功能说明

版本更新通知功能会在扩展安装、更新或浏览器启动时自动检测版本更新，并打开一个优雅的更新通知页面。

## 工作原理

1. **自动触发**：扩展在以下场景自动检测版本更新
   - 首次安装扩展
   - 扩展更新后
   - 浏览器重启时
   
2. **版本检测**：后台脚本 (background.js) 读取 manifest.json 的版本号
3. **版本比较**：使用 chrome.storage.local 存储上次版本号进行对比
4. **自动打开**：检测到新版本时自动打开 version-update.html 页面
5. **用户友好**：专门设计的更新通知页面，提供"稍后查看"和"开始使用"选项

## 文件结构

```
webapp/extension/
├── background.js               # 后台脚本，负责版本检测
├── manifest.json               # 扩展配置（包含版本号和权限）
└── pages/
    ├── version-update.html     # 版本更新通知页面
    ├── js/
    │   └── version-notifier.js # 页面内通知脚本（可选）
    └── css/
        └── main.css            # 样式文件
```

## 更新版本时的操作步骤

### 1. 更新版本号

**只需修改 manifest 文件**，版本号会自动从 manifest 读取：

**manifest.chrome.json**
```json
{
  "version": "1.0.3"  // 更新这里
}
```

**manifest.ff.json**
```json
{
  "version": "1.0.3"  // 更新这里
}
```

> 💡 **注意**：version-notifier.js 会自动从 manifest.json 读取版本号，无需手动同步！

### 2. 更新更新内容

编辑 `pages/version-update.html` 文件中的内容部分：

```html
<div class="update-section">
    <h2>✨ 新增功能</h2>
    <ul class="update-list">
        <li><strong>功能名称</strong> - 功能描述</li>
        <!-- 添加更多新功能 -->
    </ul>
</div>

<div class="update-section">
    <h2>🔧 改进优化</h2>
    <ul class="update-list">
        <li><strong>优化项</strong> - 优化说明</li>
        <!-- 添加更多改进 -->
    </ul>
</div>
```

可用的 emoji 图标：
- ✨ 新功能
- ⚡ 性能优化
- 🔧 Bug修复
- 🎨 UI改进
- 📝 文档更新
- 🚀 发布

### 3. 测试版本通知

**方法一：重新加载扩展**
1. 打开 chrome://extensions/
2. 找到 ReadPaper Utils 扩展
3. 点击"重新加载"按钮
4. 扩展会触发 onInstalled 事件并检测版本

**方法二：清除存储后重启浏览器**
1. 打开浏览器控制台
2. 执行：`chrome.storage.local.remove('readpaper_last_version')`
3. 重启浏览器
4. 扩展会在启动时检测到新版本

**方法三：查看后台日志**
1. 打开 chrome://extensions/
2. 找到 ReadPaper Utils，点击"service worker"
3. 查看控制台日志，了解版本检测过程

## 自定义样式

版本通知的样式定义在 `css/main.css` 文件的底部，可以根据需要调整：

### 主要颜色
```css
.version-update-header {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}
```

### 弹窗尺寸
```css
.version-update-modal {
  max-width: 480px;  /* 最大宽度 */
  width: 90%;         /* 相对宽度 */
}
```

### 动画速度
```css
.version-update-overlay {
  transition: opacity 0.3s ease;  /* 调整渐变速度 */
}
```

## 功能特性

### 自动触发机制
- ✅ 扩展首次安装时自动显示
- ✅ 扩展更新后自动显示
- ✅ 浏览器重启时检测并显示（如有更新）
- ✅ 无需用户手动打开页面

### 后台检测
- ✅ 使用 chrome.runtime.onInstalled 监听安装/更新事件
- ✅ 使用 chrome.runtime.onStartup 监听浏览器启动
- ✅ 使用 chrome.storage.local 持久化版本记录
- ✅ 自动比较版本号，智能判断是否显示

### 用户体验
- ✅ 延迟 500ms 显示，确保页面加载完成
- ✅ 点击遮罩层可关闭
- ✅ 点击"知道了"按钮关闭
- ✅ 优雅的淡入淡出动画

### 响应式设计
- ✅ 支持移动端和桌面端
- ✅ 自适应深色模式
- ✅ 内容可滚动（防止溢出）

### 存储管理
- ✅ 使用 localStorage 持久化版本记录
- ✅ 不依赖服务器或网络
- ✅ 可手动重置触发通知

## 禁用版本通知

如果需要临时禁用版本通知，可以：

1. **临时禁用**（控制台执行）：
```javascript
VersionNotifier.reset();
localStorage.setItem('readpaper_last_version', '999.999.999');
```

2. **永久禁用**：
从 `partials/footer.html` 中移除：
```html
<script src="js/version-notifier.js" defer></script>
```

## 兼容性

- ✅ Chrome/Edge 90+
- ✅ Firefox 88+
- ✅ Safari 14+
- ✅ 移动端浏览器

## 注意事项

1. **版本号格式**：必须使用语义化版本号（如 1.0.2）
2. **只需更新 manifest**：版本号会自动从 manifest.json 读取，无需手动同步
3. **内容长度**：更新内容不宜过长，建议 3-5 条
4. **测试验证**：每次更新后务必测试通知显示是否正常
5. **浏览器兼容**：支持 Chrome Extension API 和普通 Web 环境
