#include <u.h>
#include <libc.h>

extern uintptr _barrier(uintptr);

void
lock(Lock *lk)
{
	int i;

	/* once fast */
	if(!_tas(&lk->val))
		return;
	/* a thousand times pretty fast */
	for(i=0; i<1000; i++){
		if(!_tas(&lk->val))
			return;
		sleep(0);
	}
	/* now nice and slow */
	for(i=0; i<1000; i++){
		if(!_tas(&lk->val))
			return;
		sleep(100);
	}
	/* take your time */
	while(_tas(&lk->val))
		sleep(1000);
}

int
canlock(Lock *lk)
{
	return _tas(&lk->val) == 0;
}

void
unlock(Lock *lk)
{
	lk->val = _barrier(0);
}
