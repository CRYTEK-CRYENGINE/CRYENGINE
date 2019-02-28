#! /usr/bin/env python3
"""
Script to call uncrustify.

The idea is that it can be used from p4v or as a trigger on a build agent.
"""
import argparse
import fnmatch
import os
import subprocess
import sys
import P4

CPP_FILE_ENDINGS = {'.cpp', '.cxx', '.inl', '.h', '.hpp'}


class UncrustifyRunner:
    """
    A small class to run uncrustify in a given changelist.
    """

    def __init__(self, user, client, allowed_patterns=None, trigger=False):
        self.p4 = P4.P4()
        self.p4.user = user
        self.p4.client = client
        self.p4.exception_level = 1
        self.p4.connect()
        self.allowed_patterns = allowed_patterns if allowed_patterns else []
        self.trigger = trigger
        self.unshelve_changelist = None

    def run(self, *, changelist=None, filelist=None, check=True):
        """
        Runs uncrustify.
        """
        cmd = uncrustify_cmd(check=check)
        if changelist and not filelist:
            filelist = self.get_changelist_files(changelist, check=check)
        elif changelist and filelist:
            raise ValueError('Either use a changelist or pass the list of files.')
        success = True
        filelist = self.filter_cpp_files(filelist)
        if not filelist:
            print('No files to process.')
        else:
            for file_path in filelist:
                try:
                    subprocess.check_call(cmd + [file_path])
                except subprocess.CalledProcessError:
                    success = False
        if self.unshelve_changelist:
            self.p4.run_revert('-c', self.unshelve_changelist, '//...')
            self.p4.run_change('-d', self.unshelve_changelist)
        return success

    def create_changelist(self, description):
        """
        Creates an empty changelist. This is used when the script works as a trigger, to unshelve
        the files in an isolated changelist.

        :param description: Description text for the changelist.
        :return: The changelist number as a string.
        """
        change_spec = self.p4.fetch_change()
        change_spec['Description'] = description
        change_spec['Files'] = []
        message = self.p4.save_change(change_spec)[0]
        return message.split()[1]

    def get_changelist_files(self, changelist, *, check=True):
        """
        From a changelist number, retrieve the local paths of the files in the changelist.

        :param changelist: Changelist number.
        :return: The local paths of the files in the changelist.
        """
        description = self.p4.run_describe('-s', changelist)[0]
        if 'depotFile' not in description:
            description = self.p4.run_describe('-s', '-S', changelist)[0]
        if self.trigger:
            if 'shelved' not in description:
                raise ValueError('The changelist does not have shelved files.')
            #  HACK, do not unshelve the uncrustify script or it will fail
            if any(f.endswith('p4v_uncrustify.py') for f in description['depotFile']):
                return []
            self.unshelve_changelist = self.create_changelist('!X For style check')
            self.p4.run_unshelve('-s', changelist, '-c', self.unshelve_changelist)
        # Retrieve local paths for all files in the changelist.
        depot_paths = []
        to_checkout = []
        for depot_path, action in zip(description['depotFile'], description['action']):
            if 'delete' not in action:
                depot_paths.append(depot_path)
                if action == 'branch':
                    to_checkout.append(depot_path)
        if not check and to_checkout:
            self.p4.run_edit(to_checkout)
        where_results = self.p4.run_where(depot_paths)
        return [f['path'] for f in where_results]

    def filter_cpp_files(self, filelist):
        """
        Take the list of files and remove CryPhysics and non-C++ files.
        Uncrustify will attempt to format any file as C++ if we tell it to, which can ruin shaders, for instance.
        If patterns are set, it will apply them too to filter the files that do not match.

        :return: List of C++ files that should be formatted.
        """
        to_style = []
        for filepath in filelist:
            # CryPhysics does not receive code formatting.
            if 'cryphysics' in filepath.lower():
                continue

            # Only try to format actual C++ files.
            if os.path.splitext(filepath)[1].lower() not in CPP_FILE_ENDINGS:
                continue

            to_style.append(filepath)
        filtered_files = set()
        if self.allowed_patterns:
            for pattern in self.allowed_patterns:
                filtered_files.update(fnmatch.filter(to_style, pattern))
        else:
            filtered_files.update(to_style)
        return sorted(filtered_files)


def uncrustify_cmd(*, check=False):
    """
    Gets the command line to call uncrustify for every platform.

    :return: A list that contains the command line parameters.
    """
    script_location = os.path.dirname(os.path.abspath(__file__))
    uncrustify_location = os.path.normpath(os.path.join(script_location, '..'))
    platform_paths = {'win32': os.path.join(uncrustify_location, 'uncrustify')}
    uncrustify_path = platform_paths.get(sys.platform, 'uncrustify')
    cmd = [uncrustify_path, '-l', 'CPP', f'--config={uncrustify_location}/CryEngine_uncrustify.cfg']
    if check:
        cmd.append('--check')
    else:
        cmd.extend(['--replace', '--no-backup'])
    return cmd


def main():
    """
    Entry point.
    """
    args, filelist = parse_arguments()
    uncrustify = UncrustifyRunner(args.user, args.client, args.patterns, args.trigger)
    try:
        return not uncrustify.run(changelist=args.changelist, filelist=filelist, check=not args.apply)
    except ValueError as e:
        print(f'Error: {e}')
        return 1


def parse_arguments():
    """
    Parses the command line arguments.
    """
    parser = argparse.ArgumentParser(description='Build Template.')
    parser.add_argument('--apply', action='store_true', default=False, help='Overwrite files with formatted ones.')
    parser.add_argument('--user', default='', help='Perforce username.')
    parser.add_argument('--client', default='', help='Perforce client.')
    parser.add_argument('--changelist', default='', help='Changelist containing files on which to run Uncrustify.')
    parser.add_argument('--trigger', action='store_true', default=False,
                        help='Run as a trigger.')
    parser.add_argument('--pattern', dest='patterns', action='append', default=[],
                        help='To add one or more patterns to filter files.')
    return parser.parse_known_args()


if __name__ == "__main__":
    sys.exit(main())
