#ifndef __CLAR_LIBGIT2__
#define __CLAR_LIBGIT2__

#include "clar.h"
#include <git2.h>
#include "common.h"
#include "posix.h"
#include "oid.h"

/**
 * Replace for `clar_must_pass` that passes the last library error as the
 * test failure message.
 *
 * Use this wrapper around all `git_` library calls that return error codes!
 */
#define cl_git_pass(expr) cl_git_expect((expr), 0, __FILE__, __func__, __LINE__)

#define cl_git_fail_with(error, expr) cl_git_expect((expr), error, __FILE__, __func__, __LINE__)

#define cl_git_expect(expr, expected, file, func, line) do { \
	int _lg2_error; \
	git_error_clear(); \
	if ((_lg2_error = (expr)) != expected) \
		cl_git_report_failure(_lg2_error, expected, file, func, line, "Function call failed: " #expr); \
	} while (0)

/**
 * Wrapper for `clar_must_fail` -- this one is
 * just for consistency. Use with `git_` library
 * calls that are supposed to fail!
 */
#define cl_git_fail(expr) do { \
	if ((expr) == 0) \
		git_error_clear(), \
		cl_git_report_failure(0, 0, __FILE__, __func__, __LINE__, "Function call succeeded: " #expr); \
	} while (0)

/**
 * Like cl_git_pass, only for Win32 error code conventions
 */
#define cl_win32_pass(expr) do { \
	int _win32_res; \
	if ((_win32_res = (expr)) == 0) { \
		git_error_set(GIT_ERROR_OS, "Returned: %d, system error code: %lu", _win32_res, GetLastError()); \
		cl_git_report_failure(_win32_res, 0, __FILE__, __func__, __LINE__, "System call failed: " #expr); \
	} \
	} while(0)

/**
 * Thread safe assertions; you cannot use `cl_git_report_failure` from a
 * child thread since it will try to `longjmp` to abort and "the effect of
 * a call to longjmp() where initialization of the jmp_buf structure was
 * not performed in the calling thread is undefined."
 *
 * Instead, callers can provide a clar thread error context to a thread,
 * which will populate and return it on failure.  Callers can check the
 * status with `cl_git_thread_check`.
 */
typedef struct {
	int error;
	const char *file;
	const char *func;
	int line;
	const char *expr;
	char error_msg[4096];
} cl_git_thread_err;

#ifdef GIT_THREADS
# define cl_git_thread_pass(threaderr, expr) cl_git_thread_pass_(threaderr, (expr), __FILE__, __func__, __LINE__)
#else
# define cl_git_thread_pass(threaderr, expr) cl_git_pass(expr)
#endif

#define cl_git_thread_pass_(__threaderr, __expr, __file, __func, __line) do { \
	git_error_clear(); \
	if ((((cl_git_thread_err *)__threaderr)->error = (__expr)) != 0) { \
		const git_error *_last = git_error_last(); \
		((cl_git_thread_err *)__threaderr)->file = __file; \
		((cl_git_thread_err *)__threaderr)->func = __func; \
		((cl_git_thread_err *)__threaderr)->line = __line; \
		((cl_git_thread_err *)__threaderr)->expr = "Function call failed: " #__expr; \
		p_snprintf(((cl_git_thread_err *)__threaderr)->error_msg, 4096, "thread 0x%" PRIxZ " - error %d - %s", \
			git_thread_currentid(), ((cl_git_thread_err *)__threaderr)->error, \
			_last ? _last->message : "<no message>"); \
		git_thread_exit(__threaderr); \
	} \
	} while (0)

GIT_INLINE(void) cl_git_thread_check(void *data)
{
	cl_git_thread_err *threaderr = (cl_git_thread_err *)data;
	if (threaderr->error != 0)
		clar__assert(0, threaderr->file, threaderr->func, threaderr->line, threaderr->expr, threaderr->error_msg, 1);
}

void cl_git_report_failure(int, int, const char *, const char *, int, const char *);

#define cl_assert_at_line(expr,file,func,line) \
	clar__assert((expr) != 0, file, func, line, "Expression is not true: " #expr, NULL, 1)

GIT_INLINE(void) clar__assert_in_range(
	int lo, int val, int hi,
	const char *file, const char *func, int line,
	const char *err, int should_abort)
{
	if (lo > val || hi < val) {
		char buf[128];
		p_snprintf(buf, sizeof(buf), "%d not in [%d,%d]", val, lo, hi);
		clar__fail(file, func, line, err, buf, should_abort);
	}
}

#define cl_assert_equal_sz(sz1,sz2) do { \
	size_t __sz1 = (size_t)(sz1), __sz2 = (size_t)(sz2); \
	clar__assert_equal(__FILE__,__func__,__LINE__,#sz1 " != " #sz2, 1, "%"PRIuZ, __sz1, __sz2); \
} while (0)

#define cl_assert_in_range(L,V,H) \
	clar__assert_in_range((L),(V),(H),__FILE__,__func__,__LINE__,"Range check: " #V " in [" #L "," #H "]", 1)

#define cl_assert_equal_file(DATA,SIZE,PATH) \
	clar__assert_equal_file(DATA,SIZE,0,PATH,__FILE__,__func__,(int)__LINE__)

#define cl_assert_equal_file_ignore_cr(DATA,SIZE,PATH) \
	clar__assert_equal_file(DATA,SIZE,1,PATH,__FILE__,__func__,(int)__LINE__)

void clar__assert_equal_file(
	const char *expected_data,
	size_t expected_size,
	int ignore_cr,
	const char *path,
	const char *file,
	const char *func,
	int line);

GIT_INLINE(void) clar__assert_equal_oid(
	const char *file, const char *func, int line, const char *desc,
	const git_oid *one, const git_oid *two)
{

	if (git_oid_equal(one, two))
		return;

	if (git_oid_type(one) != git_oid_type(two)) {
		char err[64];

		snprintf(err, 64, "different oid types: %d vs %d", git_oid_type(one), git_oid_type(two));
		clar__fail(file, func, line, desc, err, 1);
	} else if (git_oid_type(one) == GIT_OID_SHA1) {
		char err[] = "\"........................................\" != \"........................................\"";
		git_oid_fmt(&err[1], one);
		git_oid_fmt(&err[47], two);

		clar__fail(file, func, line, desc, err, 1);
#ifdef GIT_EXPERIMENTAL_SHA256
	} else if (one->type == GIT_OID_SHA256) {
		char err[] = "\"................................................................\" != \"................................................................\"";

		git_oid_fmt(&err[1], one);
		git_oid_fmt(&err[71], one);

		clar__fail(file, func, line, desc, err, 1);
#endif
	} else {
		clar__fail(file, func, line, desc, "unknown oid types", 1);
	}
}

GIT_INLINE(void) clar__assert_equal_oidstr(
	const char *file, const char *func, int line, const char *desc,
	const char *one_str, const git_oid *two)
{
	git_oid one;
	git_oid_t oid_type = git_oid_type(two);

	if (git_oid_from_prefix(&one, one_str, git_oid_hexsize(oid_type), oid_type) < 0) {
		clar__fail(file, func, line, desc, "could not parse oid string", 1);
	} else {
		clar__assert_equal_oid(file, func, line, desc, &one, two);
	}
}

#define cl_assert_equal_oid(one, two) \
	clar__assert_equal_oid(__FILE__, __func__, __LINE__, \
		"OID mismatch: " #one " != " #two, (one), (two))

#define cl_assert_equal_oidstr(one_str, two) \
	clar__assert_equal_oidstr(__FILE__, __func__, __LINE__, \
		"OID mismatch: " #one_str " != " #two, (one_str), (two))

/*
 * Some utility macros for building long strings
 */
#define REP4(STR)	 STR STR STR STR
#define REP15(STR)	 REP4(STR) REP4(STR) REP4(STR) STR STR STR
#define REP16(STR)	 REP4(REP4(STR))
#define REP256(STR)  REP16(REP16(STR))
#define REP1024(STR) REP4(REP256(STR))

/* Write the contents of a buffer to disk */
void cl_git_mkfile(const char *filename, const char *content);
void cl_git_append2file(const char *filename, const char *new_content);
void cl_git_rewritefile(const char *filename, const char *new_content);
void cl_git_write2file(const char *path, const char *data,
	size_t datalen, int flags, unsigned int mode);
void cl_git_rmfile(const char *filename);

bool cl_toggle_filemode(const char *filename);
bool cl_is_chmod_supported(void);

/* Environment wrappers */
char *cl_getenv(const char *name);
bool cl_is_env_set(const char *name);
int cl_setenv(const char *name, const char *value);

/* Reliable rename */
int cl_rename(const char *source, const char *dest);

/* Git sandbox setup helpers */

git_repository *cl_git_sandbox_init(const char *sandbox);
git_repository *cl_git_sandbox_init_new(const char *name);
void cl_git_sandbox_cleanup(void);
git_repository *cl_git_sandbox_reopen(void);

/*
 * build a sandbox-relative from path segments
 * is_dir will add a trailing slash
 * vararg must be a NULL-terminated char * list
 */
const char *cl_git_sandbox_path(int is_dir, ...);

/* Local-repo url helpers */
const char* cl_git_fixture_url(const char *fixturename);
const char* cl_git_path_url(const char *path);

/* Test repository cleaner */
int cl_git_remove_placeholders(const char *directory_path, const char *filename);

/* commit creation helpers */
void cl_repo_commit_from_index(
	git_oid *out,
	git_repository *repo,
	git_signature *sig,
	git_time_t time,
	const char *msg);

/* config setting helpers */
void cl_repo_set_bool(git_repository *repo, const char *cfg, int value);
int cl_repo_get_bool(git_repository *repo, const char *cfg);

void cl_repo_set_int(git_repository *repo, const char *cfg, int value);
int cl_repo_get_int(git_repository *repo, const char *cfg);

void cl_repo_set_string(git_repository *repo, const char *cfg, const char *value);

/*
 * set up a fake "home" directory -- automatically configures cleanup
 * function to restore the home directory, although you can call it
 * explicitly if you wish (with NULL).
 */
void cl_fake_homedir(git_str *);
void cl_fake_homedir_cleanup(void *);

/*
 * set up a fake global configuration directory -- automatically
 * configures cleanup function to restore the global config
 * although you can call it explicitly if you wish (with NULL).
 */
void cl_fake_globalconfig(git_str *);
void cl_fake_globalconfig_cleanup(void *);

void cl_sandbox_set_homedir(const char *);
void cl_sandbox_set_search_path_defaults(void);
void cl_sandbox_disable_ownership_validation(void);

#ifdef GIT_WIN32
# define cl_msleep(x) Sleep(x)
#else
# define cl_msleep(x) usleep(1000 * (x))
#endif

#ifdef GIT_WIN32
bool cl_sandbox_supports_8dot3(void);
#endif

#endif
