import sharp from 'sharp'
await sharp('board.jpg')
  .resize({ width: 1600, withoutEnlargement: true })
  .webp({ quality: 72 })
  .toFile('public/board.webp')
console.log('wrote public/board.webp')
