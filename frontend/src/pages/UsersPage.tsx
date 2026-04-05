import UserList from '@/components/admin/UserList'

export default function UsersPage() {
  return (
    <div className="mx-auto max-w-2xl space-y-8 px-4 py-8 sm:px-6">
      <section>
        <h2 className="mb-1 text-xl font-extrabold tracking-tight text-slate-900">
          User <span className="bg-gradient-to-r from-indigo-600 to-violet-600 bg-clip-text text-transparent">Management</span>
        </h2>
        <p className="mb-5 text-sm text-slate-500">Manage system user accounts</p>
        <UserList />
      </section>
    </div>
  )
}
