#ifdef NDEBUG
#undef NDEBUG
#endif
#include "kforwin32.h"
#include "KDavLock.h"
#include <assert.h>
#include <stdio.h>

int main() {
    KDavLockManager manager;
    KLockToken* old = manager.new_token("test", Lock_exclusive, -1);
    assert(manager.lock("expired", old) == Lock_op_success);
    assert(manager.find_file_locked("expired") == NULL);
    KLockToken* replacement = manager.new_token("test", Lock_exclusive, 60);
    assert(manager.lock("expired", replacement) == Lock_op_success);
    assert(manager.unlock(old) == Lock_op_conflick);
    old->release();
    KLockToken* found = manager.find_file_locked("expired");
    assert(found == replacement);
    found->release();
    assert(manager.unlock(replacement) == Lock_op_success);
    replacement->release();
    KLockToken* shared = manager.new_token("test", Lock_share, -1);
    KLockToken* active = manager.new_token("test", Lock_share, 60);
    assert(manager.lock("shared", active) == Lock_op_success);
    assert(manager.lock("shared", shared) == Lock_op_success);
    found = manager.find_file_locked("shared");
    assert(found == active);
    found->release();
    // Expiration must not remove another still-active token on the same file.
    assert(manager.unlock(shared) != Lock_op_success);
    shared->release();
    assert(manager.unlock(active) == Lock_op_success);
    active->release();
    puts("WebDAV lock regression tests passed");
}
