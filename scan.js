const fs = require('fs');
const path = require('path');

const ignoreDirs = ['.github', '.git', 'node_modules'];

function generateIndexHtml(dirPath) {
  const items = fs.readdirSync(dirPath, {withFileTypes:true});
  let folders = [];
  let files = [];

  for(const item of items) {
    if(ignoreDirs.includes(item.name)) continue;
    if(item.name === 'index.html') continue;

    if(item.isDirectory()){
      folders.push(item);
    }else{
      files.push(item);
    }
  }

  let folderHtml = '';
  for(const d of folders){
    folderHtml += `<a href="${encodeURIComponent(d.name)}/" class="folder">${d.name}</a>`;
  }

  let fileHtml = '';
  for(const f of files){
    fileHtml += `<a href="${encodeURIComponent(f.name)}" class="file">${f.name}</a>`;
  }

  const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
*{box-sizing:border-box;margin:0;padding:0;font-family:system-ui, sans-serif}
body{padding:24px;max-width:1200px;margin:0 auto}
.folder-wrap{display:grid;grid-template-columns:repeat(auto-fill, minmax(160px, 1fr));gap:12px;margin-bottom:32px}
.file-wrap{display:grid;grid-template-columns:repeat(auto-fill, minmax(220px,1fr));gap:8px}
a{display:block;padding:14px 16px;border:1px solid #ddd;border-radius:8px;text-decoration:none;color:#222;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
a.folder{background:#f6f8fa}
a:hover{background:#eef2f7;border-color:#999}
</style>
</head>
<body>
<div class="folder-wrap">
${folderHtml}
</div>
<div class="file-wrap">
${fileHtml}
</div>
</body>
</html>`;

  const outPath = path.join(dirPath, 'index.html');
  fs.writeFileSync(outPath, html, 'utf8');
  console.log(`✅ 生成：${outPath}`);

  // 递归子文件夹
  for(const d of folders){
    const subDir = path.join(dirPath, d.name);
    generateIndexHtml(subDir);
  }
}

// 从仓库根目录开始扫描
generateIndexHtml('.');
console.log('\n🎉 全部目录页面生成完毕');
