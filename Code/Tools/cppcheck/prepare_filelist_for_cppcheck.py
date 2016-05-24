#!python3
import fnmatch
import os
import sys

sources = [
    {'dir': 'Code', 'exclude_masks': ['Tools/*', 'Sandbox/Editor/*', 'Libs/*', 'Game_Hunt/*', 'Game03/*', 'SDKs/*']},
]


def matches_any_of(filename, masks):
    """
    :param filename: Filename to check.
    :param masks: List of masks against which to check the filename.
    :return: True if filename matches any of the masks.
    """
    name = filename.lower().replace('\\', '/')
    for mask in masks:
        mask = mask.lower().replace('\\', '/')
        if fnmatch.fnmatch(name, mask):
            print("{} matches {}".format(name, mask))
            return True
    return False


def process_folder(out_files, folder, exclude_masks):
    """
    Walk the specified directory, runnin cppcheck against all .cpp and .c files in it,
    unless the file path matches any of the exclude_masks, in which case, skip it.
    :param out_files: [out] List of files in the specified folder that CppCheck should process.
    :param folder: Folder to parse.
    :param exclude_masks: List of filename masks to skip.
    :return: None.
    """
    for dirpath, dirnames, filenames in os.walk(folder):
        # If a directory matches an exclude path, so will all the files it contains.
        if matches_any_of(dirpath, exclude_masks):
            continue

        for filename in filenames:
            filepath = os.path.join(dirpath, filename)

            # Force lower case just incase somebody has named their file strangely (for instance, "file.C")
            if os.path.splitext(filename.lower())[1] not in ['.cpp', '.cxx', '.c']:
                continue

            # If the file path matches any of the exclude masks, we are deliberately ignoring it.
            if matches_any_of(filepath, exclude_masks):
                continue
            out_files.append(filepath)


def main(argv):
    if len(argv) != 3:
        print("Usage: python3 prepare_filelist_for_cppcheck.py base_folder output_list_filename")
        return 1

    base_folder = os.path.abspath(argv[1])

    files = []

    for src in sources:
        src['exclude_masks'] = [os.path.join(base_folder, src['dir'], x) for x in src['exclude_masks']]
        process_folder(files, os.path.join(base_folder, src['dir']), src['exclude_masks'])

    with open(argv[2], 'w') as file:
        for f in files:
            f = os.path.abspath(f)
            f = os.path.relpath(f, base_folder)
            file.write(f.replace('\\', '/') + '\n')

    return 0


if __name__ == "__main__":
    res = main(sys.argv)
    sys.exit(res)
