#!/bin/sh

dev_is_known_variant() {
	case "$1" in
	release | debug | asan)
		return 0
		;;
	*)
		return 1
		;;
	esac
}

dev_is_known_preset() {
	case "$1" in
	release | debug | asan | ci-release | nightly | release-matrix)
		return 0
		;;
	*)
		return 1
		;;
	esac
}

dev_preset_from_variant() {
	if dev_is_known_variant "$1"; then
		printf '%s\n' "$1"
		return 0
	fi

	return 1
}

dev_build_dir_for_preset() {
	_repo_root="$1"
	_preset="$2"

	case "${_preset}" in
	release | debug | asan)
		printf '%s/build/%s\n' "${_repo_root}" "${_preset}"
		;;
	ci-release)
		printf '%s/build/ci/release\n' "${_repo_root}"
		;;
	nightly)
		if [ -z "${KDAL_TARGET_TRIPLET:-}" ]; then
			return 1
		fi
		printf '%s/build/nightly/%s\n' "${_repo_root}" "${KDAL_TARGET_TRIPLET}"
		;;
	release-matrix)
		if [ -z "${KDAL_TARGET_TRIPLET:-}" ]; then
			return 1
		fi
		printf '%s/build/release/%s\n' "${_repo_root}" "${KDAL_TARGET_TRIPLET}"
		;;
	*)
		return 1
		;;
	esac
}
