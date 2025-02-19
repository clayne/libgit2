#include "clar_libgit2.h"
#include "posix.h"
#include "reset_helpers.h"
#include "path.h"
#include "futils.h"
#include "index.h"

static git_repository *repo;
static git_object *target;

void test_reset_hard__initialize(void)
{
	repo = cl_git_sandbox_init("status");
	target = NULL;
}

void test_reset_hard__cleanup(void)
{
	if (target != NULL) {
		git_object_free(target);
		target = NULL;
	}

	cl_git_sandbox_cleanup();
}

static int strequal_ignore_eol(const char *exp, const char *str)
{
	while (*exp && *str) {
		if (*exp != *str) {
			while (*exp == '\r' || *exp == '\n') ++exp;
			while (*str == '\r' || *str == '\n') ++str;
			if (*exp != *str)
				return false;
		} else {
			exp++; str++;
		}
	}
	return (!*exp && !*str);
}

void test_reset_hard__resetting_reverts_modified_files(void)
{
	git_str path = GIT_STR_INIT, content = GIT_STR_INIT;
	int i;
	static const char *files[4] = {
		"current_file",
		"modified_file",
		"staged_new_file",
		"staged_changes_modified_file" };
	static const char *before[4] = {
		"current_file\n",
		"modified_file\nmodified_file\n",
		"staged_new_file\n",
		"staged_changes_modified_file\nstaged_changes_modified_file\nstaged_changes_modified_file\n"
	};
	static const char *after[4] = {
		"current_file\n",
		"modified_file\n",
		NULL,
		"staged_changes_modified_file\n"
	};
	const char *wd = git_repository_workdir(repo);

	cl_assert(wd);

	for (i = 0; i < 4; ++i) {
		cl_git_pass(git_str_joinpath(&path, wd, files[i]));
		cl_git_pass(git_futils_readbuffer(&content, path.ptr));
		cl_assert_equal_s(before[i], content.ptr);
	}

	cl_git_pass(git_revparse_single(&target, repo, "26a125e"));

	cl_git_pass(git_reset(repo, target, GIT_RESET_HARD, NULL));

	for (i = 0; i < 4; ++i) {
		cl_git_pass(git_str_joinpath(&path, wd, files[i]));
		if (after[i]) {
			cl_git_pass(git_futils_readbuffer(&content, path.ptr));
			cl_assert(strequal_ignore_eol(after[i], content.ptr));
		} else {
			cl_assert(!git_fs_path_exists(path.ptr));
		}
	}

	git_str_dispose(&content);
	git_str_dispose(&path);
}

void test_reset_hard__cannot_reset_in_a_bare_repository(void)
{
	git_repository *bare;

	cl_git_pass(git_repository_open(&bare, cl_fixture("testrepo.git")));
	cl_assert(git_repository_is_bare(bare) == true);

	cl_git_pass(git_revparse_single(&target, bare, KNOWN_COMMIT_IN_BARE_REPO));

	cl_assert_equal_i(GIT_EBAREREPO, git_reset(bare, target, GIT_RESET_HARD, NULL));

	git_repository_free(bare);
}

static void index_entry_init(git_index *index, int side, git_oid *oid)
{
	git_index_entry entry;

	memset(&entry, 0x0, sizeof(git_index_entry));

	entry.path = "conflicting_file";
	GIT_INDEX_ENTRY_STAGE_SET(&entry, side);
	entry.mode = 0100644;
	git_oid_cpy(&entry.id, oid);

	cl_git_pass(git_index_add(index, &entry));
}

static void unmerged_index_init(git_index *index, int entries)
{
	int write_ancestor = 1;
	int write_ours = 2;
	int write_theirs = 4;
	git_oid ancestor, ours, theirs;

	git_oid_from_string(&ancestor, "452e4244b5d083ddf0460acf1ecc74db9dcfa11a", GIT_OID_SHA1);
	git_oid_from_string(&ours, "32504b727382542f9f089e24fddac5e78533e96c", GIT_OID_SHA1);
	git_oid_from_string(&theirs, "061d42a44cacde5726057b67558821d95db96f19", GIT_OID_SHA1);

	cl_git_rewritefile("status/conflicting_file", "conflicting file\n");

	if (entries & write_ancestor)
		index_entry_init(index, 1, &ancestor);

	if (entries & write_ours)
		index_entry_init(index, 2, &ours);

	if (entries & write_theirs)
		index_entry_init(index, 3, &theirs);
}

void test_reset_hard__resetting_reverts_unmerged(void)
{
	git_index *index;
	int entries;

	/* Ensure every permutation of non-zero stage entries results in the
	 * path being cleaned up. */
	for (entries = 1; entries < 8; entries++) {
		cl_git_pass(git_repository_index(&index, repo));

		unmerged_index_init(index, entries);
		cl_git_pass(git_index_write(index));

		cl_git_pass(git_revparse_single(&target, repo, "26a125e"));
		cl_git_pass(git_reset(repo, target, GIT_RESET_HARD, NULL));

		cl_assert(git_fs_path_exists("status/conflicting_file") == 0);

		git_object_free(target);
		target = NULL;

		git_index_free(index);
	}
}

void test_reset_hard__cleans_up_merge(void)
{
	git_str merge_head_path = GIT_STR_INIT,
		merge_msg_path = GIT_STR_INIT,
		merge_mode_path = GIT_STR_INIT,
		orig_head_path = GIT_STR_INIT;

	cl_git_pass(git_str_joinpath(&merge_head_path, git_repository_path(repo), "MERGE_HEAD"));
	cl_git_mkfile(git_str_cstr(&merge_head_path), "beefbeefbeefbeefbeefbeefbeefbeefbeefbeef\n");

	cl_git_pass(git_str_joinpath(&merge_msg_path, git_repository_path(repo), "MERGE_MSG"));
	cl_git_mkfile(git_str_cstr(&merge_msg_path), "Merge commit 0017bd4ab1ec30440b17bae1680cff124ab5f1f6\n");

	cl_git_pass(git_str_joinpath(&merge_mode_path, git_repository_path(repo), "MERGE_MODE"));
	cl_git_mkfile(git_str_cstr(&merge_mode_path), "");

	cl_git_pass(git_str_joinpath(&orig_head_path, git_repository_path(repo), "ORIG_HEAD"));
	cl_git_mkfile(git_str_cstr(&orig_head_path), "0017bd4ab1ec30440b17bae1680cff124ab5f1f6");

	cl_git_pass(git_revparse_single(&target, repo, "0017bd4"));
	cl_git_pass(git_reset(repo, target, GIT_RESET_HARD, NULL));

	cl_assert(!git_fs_path_exists(git_str_cstr(&merge_head_path)));
	cl_assert(!git_fs_path_exists(git_str_cstr(&merge_msg_path)));
	cl_assert(!git_fs_path_exists(git_str_cstr(&merge_mode_path)));

	cl_assert(git_fs_path_exists(git_str_cstr(&orig_head_path)));
	cl_git_pass(p_unlink(git_str_cstr(&orig_head_path)));

	git_str_dispose(&merge_head_path);
	git_str_dispose(&merge_msg_path);
	git_str_dispose(&merge_mode_path);
	git_str_dispose(&orig_head_path);
}

void test_reset_hard__reflog_is_correct(void)
{
	git_str buf = GIT_STR_INIT;
	git_annotated_commit *annotated;
	const char *exp_msg = "commit: Add a file which name should appear before the "
		"\"subdir/\" folder while being dealt with by the treewalker";

	reflog_check(repo, "HEAD", 3, "emeric.fermas@gmail.com", exp_msg);
	reflog_check(repo, "refs/heads/master", 3, "emeric.fermas@gmail.com", exp_msg);

	/* Branch not moving, no reflog entry */
	cl_git_pass(git_revparse_single(&target, repo, "HEAD^{commit}"));
	cl_git_pass(git_reset(repo, target, GIT_RESET_HARD, NULL));
	reflog_check(repo, "HEAD", 3, "emeric.fermas@gmail.com", exp_msg);
	reflog_check(repo, "refs/heads/master", 3, "emeric.fermas@gmail.com", exp_msg);

	git_object_free(target);

	/* Moved branch, expect id in message */
	cl_git_pass(git_revparse_single(&target, repo, "HEAD~^{commit}"));
	cl_git_pass(git_str_printf(&buf, "reset: moving to %s", git_oid_tostr_s(git_object_id(target))));
	cl_git_pass(git_reset(repo, target, GIT_RESET_HARD, NULL));
	reflog_check(repo, "HEAD", 4, NULL, git_str_cstr(&buf));
	reflog_check(repo, "refs/heads/master", 4, NULL, git_str_cstr(&buf));

	git_str_dispose(&buf);

	/* Moved branch, expect revspec in message */
	exp_msg = "reset: moving to HEAD~^{commit}";
	cl_git_pass(git_annotated_commit_from_revspec(&annotated, repo, "HEAD~^{commit}"));
	cl_git_pass(git_reset_from_annotated(repo, annotated, GIT_RESET_HARD, NULL));
	reflog_check(repo, "HEAD", 5, NULL, exp_msg);
	reflog_check(repo, "refs/heads/master", 5, NULL, exp_msg);

	git_annotated_commit_free(annotated);

}

void test_reset_hard__switch_file_to_dir(void)
{
	git_index_entry entry = {{ 0 }};
	git_index *idx;
	git_odb *odb;
	git_object *commit;
	git_tree *tree;
	git_signature *sig;
	git_oid src_tree_id, tgt_tree_id;
	git_oid src_id, tgt_id;
	git_index_options index_opts = GIT_INDEX_OPTIONS_INIT;

	cl_git_pass(git_repository_odb(&odb, repo));
	cl_git_pass(git_odb_write(&entry.id, odb, "", 0, GIT_OBJECT_BLOB));
	git_odb_free(odb);

	entry.mode = GIT_FILEMODE_BLOB;
	cl_git_pass(git_index_new_ext(&idx, &index_opts));
	cl_git_pass(git_signature_now(&sig, "foo", "bar"));

	/* Create the old tree */
	entry.path = "README";
	cl_git_pass(git_index_add(idx, &entry));
	entry.path = "dir";
	cl_git_pass(git_index_add(idx, &entry));

	cl_git_pass(git_index_write_tree_to(&src_tree_id, idx, repo));
	cl_git_pass(git_index_clear(idx));

	cl_git_pass(git_tree_lookup(&tree, repo, &src_tree_id));
	cl_git_pass(git_commit_create(&src_id, repo, NULL, sig, sig, NULL, "foo", tree, 0, NULL));
	git_tree_free(tree);

	/* Create the new tree */
	entry.path = "README";
	cl_git_pass(git_index_add(idx, &entry));
	entry.path = "dir/FILE";
	cl_git_pass(git_index_add(idx, &entry));

	cl_git_pass(git_index_write_tree_to(&tgt_tree_id, idx, repo));
	cl_git_pass(git_tree_lookup(&tree, repo, &tgt_tree_id));
	cl_git_pass(git_commit_create(&tgt_id, repo, NULL, sig, sig, NULL, "foo", tree, 0, NULL));
	git_tree_free(tree);
	git_index_free(idx);
	git_signature_free(sig);

	/* Let's go to a known state of the src commit with the file named 'dir' */
	cl_git_pass(git_object_lookup(&commit, repo, &src_id, GIT_OBJECT_COMMIT));
	cl_git_pass(git_reset(repo, commit, GIT_RESET_HARD, NULL));
	git_object_free(commit);

	/* And now we move over to the commit with the directory named 'dir' */
	cl_git_pass(git_object_lookup(&commit, repo, &tgt_id, GIT_OBJECT_COMMIT));
	cl_git_pass(git_reset(repo, commit, GIT_RESET_HARD, NULL));
	git_object_free(commit);
}
