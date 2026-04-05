import DiskList from '@/components/admin/DiskList'

export default function DisksPage() {
  return (
    <div className="mx-auto max-w-2xl space-y-8 px-4 py-8 sm:px-6">
      <section>
        <h2 className="mb-1 text-xl font-extrabold tracking-tight text-slate-900">
          Disk <span className="bg-gradient-to-r from-indigo-600 to-violet-600 bg-clip-text text-transparent">Management</span>
        </h2>
        <p className="mb-5 text-sm text-slate-500">View and manage block devices and mount points</p>
        <DiskList />
      </section>
    </div>
  )
}
