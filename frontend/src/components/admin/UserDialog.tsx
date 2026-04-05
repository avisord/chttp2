import { useState, useEffect } from 'react'
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogDescription,
  DialogFooter,
  DialogClose,
} from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import type { SystemUser } from '@/types/api'

interface UserDialogProps {
  open: boolean
  onOpenChange: (open: boolean) => void
  user?: SystemUser | null
  onSubmit: (data: { username: string; password: string; shell?: string } | { password?: string; shell?: string; groups?: string }) => void
}

export default function UserDialog({ open, onOpenChange, user, onSubmit }: UserDialogProps) {
  const isEdit = !!user
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [shell, setShell] = useState('/bin/bash')
  const [groups, setGroups] = useState('')

  useEffect(() => {
    if (open) {
      if (user) {
        setUsername(user.username)
        setShell(user.shell)
        setGroups(user.groups.join(','))
        setPassword('')
      } else {
        setUsername('')
        setPassword('')
        setShell('/bin/bash')
        setGroups('')
      }
    }
  }, [open, user])

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    if (isEdit) {
      const data: { password?: string; shell?: string; groups?: string } = {}
      if (password) data.password = password
      if (shell !== user!.shell) data.shell = shell
      if (groups !== user!.groups.join(',')) data.groups = groups
      onSubmit(data)
    } else {
      onSubmit({ username, password, shell })
    }
    onOpenChange(false)
  }

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="sm:max-w-md">
        <form onSubmit={handleSubmit}>
          <DialogHeader>
            <DialogTitle>{isEdit ? 'Edit User' : 'Add User'}</DialogTitle>
            <DialogDescription>
              {isEdit ? `Modify settings for ${user!.username}` : 'Create a new system user'}
            </DialogDescription>
          </DialogHeader>
          <div className="space-y-4 py-4">
            {!isEdit && (
              <div>
                <label className="mb-1.5 block text-sm font-medium text-slate-700">Username</label>
                <Input
                  value={username}
                  onChange={(e) => setUsername(e.target.value)}
                  placeholder="username"
                  required
                  pattern="[a-zA-Z0-9_-]+"
                  autoFocus
                />
              </div>
            )}
            <div>
              <label className="mb-1.5 block text-sm font-medium text-slate-700">
                Password {isEdit && <span className="text-slate-400">(leave empty to keep current)</span>}
              </label>
              <Input
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                placeholder={isEdit ? 'unchanged' : 'password'}
                required={!isEdit}
                autoFocus={isEdit}
              />
            </div>
            <div>
              <label className="mb-1.5 block text-sm font-medium text-slate-700">Shell</label>
              <Input
                value={shell}
                onChange={(e) => setShell(e.target.value)}
                placeholder="/bin/bash"
              />
            </div>
            {isEdit && (
              <div>
                <label className="mb-1.5 block text-sm font-medium text-slate-700">
                  Groups <span className="text-slate-400">(comma-separated)</span>
                </label>
                <Input
                  value={groups}
                  onChange={(e) => setGroups(e.target.value)}
                  placeholder="wheel,docker"
                />
              </div>
            )}
          </div>
          <DialogFooter>
            <DialogClose render={<Button variant="outline" />}>Cancel</DialogClose>
            <Button type="submit">{isEdit ? 'Save Changes' : 'Create User'}</Button>
          </DialogFooter>
        </form>
      </DialogContent>
    </Dialog>
  )
}
