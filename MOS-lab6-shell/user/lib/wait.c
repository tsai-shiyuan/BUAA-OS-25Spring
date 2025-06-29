#include <env.h>
#include <lib.h>

int wait(u_int envid) {
	const volatile struct Env *e;

	e = &envs[ENVX(envid)];
	while (e->env_id == envid && e->env_status != ENV_FREE) {
		syscall_yield();
	}
	int ans = syscall_wait(envid);
	// debugf("wait ans is: %d\n", ans);
	return ans;

	//return syscall_wait(envid);
}
