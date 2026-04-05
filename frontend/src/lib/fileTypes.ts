export type FileViewType = 'image' | 'video' | 'audio' | 'pdf' | 'text' | 'unsupported'

const textExtensions = new Set([
  'txt', 'md', 'markdown', 'rst', 'log', 'csv', 'tsv',
  'json', 'xml', 'yaml', 'yml', 'toml', 'ini', 'cfg', 'conf',
  'js', 'jsx', 'ts', 'tsx', 'mjs', 'cjs',
  'py', 'rb', 'go', 'rs', 'java', 'c', 'h', 'cpp', 'hpp', 'cc',
  'cs', 'swift', 'kt', 'scala', 'lua', 'pl', 'pm', 'r',
  'sh', 'bash', 'zsh', 'fish', 'bat', 'ps1', 'cmd',
  'html', 'htm', 'css', 'scss', 'sass', 'less',
  'sql', 'graphql', 'gql',
  'makefile', 'dockerfile', 'gitignore', 'gitattributes',
  'env', 'editorconfig', 'prettierrc', 'eslintrc',
  'tex', 'bib', 'srt', 'vtt', 'asm', 's',
])

export function getFileViewType(name: string, mime: string): FileViewType {
  if (mime.startsWith('image/')) return 'image'
  if (mime.startsWith('video/')) return 'video'
  if (mime.startsWith('audio/')) return 'audio'
  if (mime === 'application/pdf') return 'pdf'

  if (mime.startsWith('text/')) return 'text'
  if (mime === 'application/json' || mime === 'application/xml') return 'text'

  const ext = name.includes('.') ? name.split('.').pop()!.toLowerCase() : name.toLowerCase()
  if (textExtensions.has(ext)) return 'text'

  return 'unsupported'
}

export function getDownloadUrl(path: string, inline = false): string {
  const url = `/fs/download?path=${encodeURIComponent(path)}`
  return inline ? `${url}&inline=1` : url
}
