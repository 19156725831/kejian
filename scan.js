const fs = require('fs');
const path = require('path');

const ignoreList = ['.git', 'node_modules', '.github'];

// 生成单目录的index.html
function genIndexHtml(dirPath) {
    const items = fs.readdirSync(dirPath, { withFileTypes: true });
    const folders = [];
    for (const item of items) {
        if (!item.isDirectory()) continue;
        if (ignoreList.includes(item.name)) continue;
        folders.push(item.name);
    }
    folders.sort();

    // 生成html内容
    const links = folders.map(name => `<a href="${encodeURI(name)}/">${name}</a>`).join('\n');
    const html = `<!DOCTYPE html>
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
${links}
</div>
</body>
</html>`;
    // 写入当前目录index.html
    const outPath = path.join(dirPath, "index.html");
    fs.writeFileSync(outPath, html, "utf8");
    console.log(`生成: ${outPath}`);
}

// 递归遍历所有目录
function walkDir(baseDir) {
    genIndexHtml(baseDir);
    const items = fs.readdirSync(baseDir, { withFileTypes: true });
    for (const item of items) {
        if (!item.isDirectory()) continue;
        if (ignoreList.includes(item.name)) continue;
        const subPath = path.join(baseDir, item.name);
        walkDir(subPath);
    }
}

// 从根目录开始扫描生成所有index.html
walkDir(__dirname);
console.log("\n✅ 全部目录 index.html 生成完毕");
