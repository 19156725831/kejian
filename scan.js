const fs = require('fs');
const path = require('path');

// 忽略列表：不想展示的目录
const ignoreList = ['.git', 'node_modules', '.github'];

/**
 * 递归读取一级子文件夹（只取当前根目录下文件夹，不递归深层）
 */
function scanRootDir(dir) {
  const items = fs.readdirSync(dir, { withFileTypes: true });
  const folders = [];
  for (const item of items) {
    if (!item.isDirectory()) continue;
    if (ignoreList.includes(item.name)) continue;
    folders.push(item.name);
  }
  // 文件夹名称排序
  folders.sort();
  return folders;
}

function buildHtml(folderList) {
  const items = folderList
    .map(name => `<a href="${encodeURI(name)}/">${name}</a>`)
    .join('\n');

  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{padding:2rem;}
.container{display:flex;flex-wrap:wrap;gap:16px;}
a{
  padding:12px 20px;
  border:1px solid #ddd;
  border-radius:6px;
  text-decoration:none;
  color:#222;
}
a:hover{
  background:#f5f5f5;
  border-color:#bbb;
}
</style>
</head>
<body>
<div class="container">
${items}
</div>
</body>
</html>`;
}

// 主逻辑
const folders = scanRootDir(__dirname);
const html = buildHtml(folders);
fs.writeFileSync('index.html', html, 'utf8');
console.log('✅ index.html 已生成，共', folders.length, '个文件夹');
