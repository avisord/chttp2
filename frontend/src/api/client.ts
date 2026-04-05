export class ApiError extends Error {
  constructor(
    message: string,
    public status: number,
  ) {
    super(message)
    this.name = 'ApiError'
  }
}

export async function apiRequest<T>(
  path: string,
  options: RequestInit = {},
): Promise<T> {
  const res = await fetch(path, {
    credentials: 'include',
    ...options,
    headers: {
      'Content-Type': 'application/json',
      ...options.headers,
    },
  })

  if (res.status === 401) {
    window.dispatchEvent(new CustomEvent('auth:unauthorized'))
    throw new ApiError('Unauthorized', 401)
  }

  if (!res.ok) {
    const body = await res.json().catch(() => ({ error: res.statusText }))
    throw new ApiError(body.error || res.statusText, res.status)
  }

  return res.json()
}

export async function apiRequestRaw(
  path: string,
  options: RequestInit = {},
): Promise<Response> {
  const res = await fetch(path, {
    credentials: 'include',
    ...options,
  })

  if (res.status === 401) {
    window.dispatchEvent(new CustomEvent('auth:unauthorized'))
    throw new ApiError('Unauthorized', 401)
  }

  if (!res.ok) {
    const body = await res.json().catch(() => ({ error: res.statusText }))
    throw new ApiError(body.error || res.statusText, res.status)
  }

  return res
}
