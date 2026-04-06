import { useEffect, useState, useRef } from 'react'
import { UserPlus } from 'lucide-react'
import { toast } from 'sonner'
import { Button } from '@/components/ui/button'
import { Skeleton } from '@/components/ui/skeleton'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { fetchUsersThunk, createUserThunk, editUserThunk, deleteUserThunk } from '@/store/slices/usersSlice'
import type { SystemUser } from '@/types/api'
import UserCard from './UserCard'
import UserDialog from './UserDialog'
import ConfirmDialog from './ConfirmDialog'

export default function UserList() {
  const dispatch = useAppDispatch()
  const { users, status } = useAppSelector((s) => s.users)
  const [dialogOpen, setDialogOpen] = useState(false)
  const [editTarget, setEditTarget] = useState<SystemUser | null>(null)
  const [deleteConfirmOpen, setDeleteConfirmOpen] = useState(false)
  const [deleteTarget, setDeleteTarget] = useState('')
  const [editConfirmOpen, setEditConfirmOpen] = useState(false)
  const pendingEditData = useRef<{ password?: string; shell?: string; groups?: string } | null>(null)

  useEffect(() => {
    dispatch(fetchUsersThunk())
  }, [dispatch])

  const handleCreate = async (data: { username: string; password: string; shell?: string }) => {
    try {
      await dispatch(createUserThunk(data)).unwrap()
      toast.success(`User "${data.username}" created`)
    } catch {
      toast.error('Failed to create user')
    }
  }

  const requestEdit = (data: { password?: string; shell?: string; groups?: string }) => {
    pendingEditData.current = data
    setEditConfirmOpen(true)
  }

  const confirmEdit = async () => {
    if (!editTarget || !pendingEditData.current) return
    try {
      await dispatch(editUserThunk({ username: editTarget.username, data: pendingEditData.current })).unwrap()
      toast.success(`User "${editTarget.username}" updated`)
    } catch {
      toast.error('Failed to update user')
    }
    pendingEditData.current = null
  }

  const confirmDelete = async () => {
    if (!deleteTarget) return
    try {
      await dispatch(deleteUserThunk(deleteTarget)).unwrap()
      toast.success(`User "${deleteTarget}" deleted`)
    } catch {
      toast.error('Failed to delete user')
    }
  }

  if (status === 'loading' && !users.length) {
    return (
      <div className="space-y-3">
        {Array.from({ length: 3 }).map((_, i) => <Skeleton key={i} className="h-16 rounded-xl" />)}
      </div>
    )
  }

  return (
    <div>
      <div className="mb-4 flex justify-end">
        <Button size="sm" onClick={() => { setEditTarget(null); setDialogOpen(true) }}>
          <UserPlus className="mr-2 h-4 w-4" />
          Add User
        </Button>
      </div>

      {!users.length ? (
        <p className="text-sm text-muted-foreground">No users found</p>
      ) : (
        <div className="space-y-2">
          {users.map((user) => (
            <UserCard
              key={user.username}
              user={user}
              onEdit={() => { setEditTarget(user); setDialogOpen(true) }}
              onDelete={() => { setDeleteTarget(user.username); setDeleteConfirmOpen(true) }}
            />
          ))}
        </div>
      )}

      <UserDialog
        open={dialogOpen}
        onOpenChange={setDialogOpen}
        user={editTarget}
        onSubmit={(data) => {
          if (editTarget) {
            requestEdit(data as { password?: string; shell?: string; groups?: string })
          } else {
            handleCreate(data as { username: string; password: string; shell?: string })
          }
        }}
      />

      <ConfirmDialog
        open={deleteConfirmOpen}
        onOpenChange={setDeleteConfirmOpen}
        title={`Delete user "${deleteTarget}"?`}
        description="This will permanently remove the system user account. This action cannot be undone."
        confirmLabel="Delete User"
        destructive
        onConfirm={confirmDelete}
      />

      <ConfirmDialog
        open={editConfirmOpen}
        onOpenChange={setEditConfirmOpen}
        title={`Update user "${editTarget?.username}"?`}
        description="This will modify the system user account settings (password, shell, or groups)."
        confirmLabel="Save Changes"
        onConfirm={confirmEdit}
      />
    </div>
  )
}
