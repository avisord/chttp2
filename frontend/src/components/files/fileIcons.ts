import materialManifest from 'material-icon-theme/dist/material-icons.json'

type FileKind = 'file' | 'dir'

export interface FileIconSpec {
  src: string
}

interface ResolveFileIconInput {
  name: string
  type: FileKind
  mime?: string
}

interface ThemeManifest {
  file?: string
  folder?: string
  fileNames?: Record<string, string>
  fileExtensions?: Record<string, string>
  folderNames?: Record<string, string>
  rootFolderNames?: Record<string, string>
  iconDefinitions?: Record<string, { iconPath: string }>
}

const manifest = materialManifest as ThemeManifest

const themeIconModules = import.meta.glob('../../../node_modules/material-icon-theme/icons/*.svg', {
  eager: true,
  import: 'default',
  query: '?url',
}) as Record<string, string>

const themeIconUrls = Object.fromEntries(
  Object.entries(themeIconModules).map(([path, url]) => [path.split('/').pop() ?? path, url]),
)

function getExtensionCandidates(name: string): string[] {
  const parts = name.toLowerCase().split('.').filter(Boolean)
  if (parts.length < 2) return []

  const candidates: string[] = []
  for (let depth = parts.length - 1; depth >= 1; depth -= 1) {
    candidates.push(parts.slice(-depth).join('.'))
  }

  return candidates
}

function getIconUrl(iconName: string | undefined, fallback: string): string {
  const resolvedName = iconName ?? fallback
  const iconPath = manifest.iconDefinitions?.[resolvedName]?.iconPath
  const fileName = iconPath?.split('/').pop() ?? `${resolvedName}.svg`
  return themeIconUrls[fileName] ?? themeIconUrls[`${fallback}.svg`]
}

function resolveFolderIconName(name: string): string | undefined {
  const normalizedName = name.toLowerCase()
  return (
    manifest.rootFolderNames?.[normalizedName] ??
    manifest.folderNames?.[normalizedName] ??
    manifest.folder
  )
}

function resolveFileIconName(name: string): string | undefined {
  const normalizedName = name.toLowerCase()
  const exactMatch = manifest.fileNames?.[normalizedName]
  if (exactMatch) return exactMatch

  for (const candidate of getExtensionCandidates(normalizedName)) {
    const extensionMatch = manifest.fileExtensions?.[candidate]
    if (extensionMatch) return extensionMatch
  }

  return manifest.file
}

export function resolveFileIcon({ name, type }: ResolveFileIconInput): FileIconSpec {
  const iconName = type === 'dir' ? resolveFolderIconName(name) : resolveFileIconName(name)
  const fallback = type === 'dir' ? 'folder' : 'file'

  return {
    src: getIconUrl(iconName, fallback),
  }
}
