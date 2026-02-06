#include <errno.h>
#include <reent.h>
#include <defines/psp_defines.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <kubridge.h>

int _newlib_heap_memblock;
unsigned _newlib_heap_size;
char *_newlib_heap_base, *_newlib_heap_end, *_newlib_heap_cur;
static char _newlib_sbrk_mutex[32] __attribute__ ((aligned (8)));

static int _newlib_vm_memblock;

extern int _newlib_heap_size_user __attribute__((weak));
extern int _newlib_vm_size_user __attribute__((weak));

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

void * _sbrk_r(struct _reent *reent, ptrdiff_t incr) {
	if (sceKernelLockLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, 1, 0) < 0)
		goto fail;
	if (!_newlib_heap_base || _newlib_heap_cur + incr >= _newlib_heap_end) {
		sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, 1);
fail:
		reent->_errno = ENOMEM;
		return (void*)-1;
	}

	char *prev_heap_end = _newlib_heap_cur;
	_newlib_heap_cur += incr;

	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, 1);
	return (void*) prev_heap_end;
}

void _init_vita_heap(void) {

	int _newlib_vm_size = 0;
	if (&_newlib_vm_size_user != NULL) {
		SceKernelAllocMemBlockKernelOpt opt;
		memset(&opt, 0, sizeof(SceKernelAllocMemBlockKernelOpt));
		opt.size = sizeof(SceKernelAllocMemBlockKernelOpt);
		opt.attr = 0x1;
		opt.field_C = 0xA8000000;
		_newlib_vm_size = ALIGN(_newlib_vm_size_user, 0x100000);
		_newlib_vm_memblock = kuKernelAllocMemBlock("rx_block", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, _newlib_vm_size, &opt);

		if (_newlib_vm_memblock < 0) {
			sceClibPrintf("Dynarec block allocation failed %x\n", _newlib_vm_memblock);
		} else {
			void *vm_addr;
			sceKernelGetMemBlockBase(_newlib_vm_memblock, (void **)&vm_addr);
			kuKernelMemProtect(vm_addr, _newlib_vm_size, KU_KERNEL_PROT_EXEC | KU_KERNEL_PROT_WRITE | KU_KERNEL_PROT_READ);
		}
	}else{
		_newlib_vm_memblock = 0;
	}

	// Create a mutex to use inside _sbrk_r
	if (sceKernelCreateLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, "sbrk mutex", 0, 0, 0) < 0) {
		goto failure;
	}

	// Always allocating the max available USER_RW mem on the system
	SceKernelFreeMemorySizeInfo info;
	info.size = sizeof(SceKernelFreeMemorySizeInfo);
	sceKernelGetFreeMemorySize(&info);

	sceClibPrintf("sceKernelGetFreeMemorySize %x\n", info.size_user);

	_newlib_heap_size = info.size_user - 24 * 1024 * 1024;

	_newlib_heap_memblock = sceKernelAllocMemBlock("Newlib heap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, _newlib_heap_size, 0);
	if (_newlib_heap_memblock < 0) {
		sceClibPrintf("Failed to alloc heap %x\n", _newlib_heap_memblock);
		goto failure;
	}
	if (sceKernelGetMemBlockBase(_newlib_heap_memblock, (void**)&_newlib_heap_base) < 0) {
		goto failure;
	}
	_newlib_heap_end = _newlib_heap_base + _newlib_heap_size;
	_newlib_heap_cur = _newlib_heap_base;

	return;
failure:
	_newlib_vm_memblock = 0;
	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}

int getVMBlock(){
  return _newlib_vm_memblock;
}

void _free_vita_heap(void) {

	// Destroy the sbrk mutex
	sceKernelDeleteLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex);

	// Free the heap memblock to avoid memory leakage.
	sceKernelFreeMemBlock(_newlib_heap_memblock);

	if (_newlib_vm_memblock > 0)
		sceKernelFreeMemBlock(_newlib_vm_memblock);

	_newlib_vm_memblock = 0;
	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}
