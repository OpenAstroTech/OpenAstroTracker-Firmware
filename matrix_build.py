#!/usr/bin/env python3

import copy
import os
import shutil
import signal
import click
import sys
from pathlib import Path
from typing import List

import tabulate
from constraint import *

from matrix_build_parallel import Executor, execute, get_available_executor_idx, get_finished_executor_idx, \
    cleanup_tempdirs, create_executors, get_source_files_to_link, wait_for_executor_to_finish, copy_caches_to_executors

CONTINUE_ON_ERROR = False

BOARDS = [
    "mksgenlv21",
    "mksgenlv2",
    "mksgenlv1",
    "esp32",
    "ramps",
]

STEPPER_TYPES = [
    "STEPPER_TYPE_NONE",
    "STEPPER_TYPE_ENABLED",
]

DRIVER_TYPES = [
    "DRIVER_TYPE_NONE",
    "DRIVER_TYPE_A4988_GENERIC",
    "DRIVER_TYPE_TMC2209_STANDALONE",
    "DRIVER_TYPE_TMC2209_UART",
]

BOOLEAN_VALUES = [0, 1]

DISPLAY_TYPES = [
    "DISPLAY_TYPE_NONE",
    "DISPLAY_TYPE_LCD_KEYPAD",
    "DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008",
    "DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017",
    "DISPLAY_TYPE_LCD_JOY_I2C_SSD1306",
]

BUILD_FLAGS = {
    "CONFIG_VERSION": "1",
    "RA_STEPPER_TYPE": [x for x in STEPPER_TYPES if x != "STEPPER_TYPE_NONE"],
    "RA_DRIVER_TYPE": [x for x in DRIVER_TYPES if x != "DRIVER_TYPE_NONE"],
    "DEC_STEPPER_TYPE": STEPPER_TYPES,
    "DEC_DRIVER_TYPE": DRIVER_TYPES,
    "USE_GPS": BOOLEAN_VALUES,
    "USE_GYRO_LEVEL": BOOLEAN_VALUES,
    "AZ_STEPPER_TYPE": STEPPER_TYPES,
    "AZ_DRIVER_TYPE": DRIVER_TYPES,
    "ALT_STEPPER_TYPE": STEPPER_TYPES,
    "ALT_DRIVER_TYPE": DRIVER_TYPES,
    "FOCUS_STEPPER_TYPE": STEPPER_TYPES,
    "FOCUS_DRIVER_TYPE": DRIVER_TYPES,
    "DISPLAY_TYPE": DISPLAY_TYPES,
    "TEST_VERIFY_MODE": BOOLEAN_VALUES,
    "DEBUG_LEVEL": ["DEBUG_NONE", "DEBUG_ANY"],
    "RA_MOTOR_CURRENT_RATING": "1",
    "RA_OPERATING_CURRENT_SETTING": "1",
    "DEC_MOTOR_CURRENT_RATING": "1",
    "DEC_OPERATING_CURRENT_SETTING": "1",
    "ALT_MOTOR_CURRENT_RATING": "1",
    "ALT_OPERATING_CURRENT_SETTING": "1",
    "AZ_MOTOR_CURRENT_RATING": "1",
    "AZ_OPERATING_CURRENT_SETTING": "1",
    "FOCUS_MOTOR_CURRENT_RATING": "1",
    "FOCUS_OPERATING_CURRENT_SETTING": "1",
    # Not all boards define the pins, so just hardcode it for compile testing
    "RA_HOMING_SENSOR_PIN": "1",
    "DEC_HOMING_SENSOR_PIN": "1",
    "RA_ENDSWITCH_EAST_SENSOR_PIN": "1",
    "RA_ENDSWITCH_WEST_SENSOR_PIN": "1",
    "DEC_ENDSWITCH_UP_SENSOR_PIN": "1",
    "DEC_ENDSWITCH_DOWN_SENSOR_PIN": "1",
}

STEPPER_SUPPORT = {
    "STEPPER_TYPE_NONE": {
        "DRIVER_TYPE_NONE"
    },
    "STEPPER_TYPE_ENABLED": {
        "DRIVER_TYPE_A4988_GENERIC",
        "DRIVER_TYPE_TMC2209_STANDALONE",
        "DRIVER_TYPE_TMC2209_UART",
    }
}


def update_dict(orig, patch):
    result = copy.deepcopy(orig)
    result.update(patch)
    return result


BOARD_SUPPORT = {
    "esp32": update_dict(BUILD_FLAGS, {
        "USE_GPS": [0],
        "USE_GYRO_LEVEL": [0],
        "DISPLAY_TYPE": [
            "DISPLAY_TYPE_NONE",
            "DISPLAY_TYPE_LCD_JOY_I2C_SSD1306"
        ],
        "AZ_DRIVER_TYPE": [
            "DRIVER_TYPE_NONE"
        ],
        "AZ_STEPPER_TYPE": [
            "STEPPER_TYPE_NONE"
        ],
        "ALT_DRIVER_TYPE": [
            "DRIVER_TYPE_NONE"
        ],
        "ALT_STEPPER_TYPE": [
            "STEPPER_TYPE_NONE"
        ],
        "FOCUS_DRIVER_TYPE": [
            "DRIVER_TYPE_NONE"
        ],
        "FOCUS_STEPPER_TYPE": [
            "STEPPER_TYPE_NONE"
        ],
    }),
    "mksgenlv21": update_dict(BUILD_FLAGS, {
        "USE_GPS": [0],
        "USE_GYRO_LEVEL": [0],
        "DISPLAY_TYPE": [
            "DISPLAY_TYPE_NONE",
            "DISPLAY_TYPE_LCD_KEYPAD"
        ],
    }),
    "mksgenlv2": update_dict(BUILD_FLAGS, {
        "USE_GPS": [0],
        "USE_GYRO_LEVEL": [0],
        "DISPLAY_TYPE": [
            "DISPLAY_TYPE_NONE",
            "DISPLAY_TYPE_LCD_KEYPAD"
        ],
    }),
    "mksgenlv1": update_dict(BUILD_FLAGS, {
        "USE_GPS": [0],
        "USE_GYRO_LEVEL": [0],
        "DISPLAY_TYPE": [
            "DISPLAY_TYPE_NONE",
            "DISPLAY_TYPE_LCD_KEYPAD"
        ],
        "FOCUS_DRIVER_TYPE": [
            "DRIVER_TYPE_NONE"
        ],
        "FOCUS_STEPPER_TYPE": [
            "STEPPER_TYPE_NONE"
        ],
    }),
    "ramps": update_dict(BUILD_FLAGS, {
        "USE_GPS": [0],
        "USE_GYRO_LEVEL": [0],
        "DISPLAY_TYPE": [
            "DISPLAY_TYPE_NONE",
            "DISPLAY_TYPE_LCD_KEYPAD"
        ],
    }),
}

SHORT_STRINGS = {
    0: "DISABLED",
    1: "ENABLED",
    "STEPPER_TYPE_NONE": "NONE",
    "STEPPER_TYPE_28BYJ48": "28BYJ48",
    "STEPPER_TYPE_NEMA17": "NEMA17",
    "DRIVER_TYPE_NONE": "NONE",
    "DRIVER_TYPE_A4988_GENERIC": "A4988_GENERIC",
    "DRIVER_TYPE_TMC2209_STANDALONE": "TMC2209_STANDALONE",
    "DRIVER_TYPE_TMC2209_UART": "TMC2209_UART",
    "DISPLAY_TYPE_NONE": "NONE",
    "DISPLAY_TYPE_LCD_KEYPAD": "LCD_KEYPAD",
    "DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008": "LCD_KEYPAD_I2C_MCP23008",
    "DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017": "LCD_KEYPAD_I2C_MCP23017",
    "DISPLAY_TYPE_LCD_JOY_I2C_SSD1306": "LCD_JOY_I2C_SSD1306",
    "RA_STEPPER_TYPE": "RA_STEPPER",
    "RA_DRIVER_TYPE": "RA_DRIVER",
    "DEC_STEPPER_TYPE": "DEC_STEPPER",
    "DEC_DRIVER_TYPE": "DEC_DRIVER",
    "USE_GPS": "GPS",
    "USE_GYRO_LEVEL": "GYRO",
    "AZ_STEPPER_TYPE": "AZ_STEPPER",
    "AZ_DRIVER_TYPE": "AZ_DRIVER",
    "ALT_STEPPER_TYPE": "ALT_STEPPER",
    "ALT_DRIVER_TYPE": "ALT_DRIVER",
    "FOCUS_STEPPER_TYPE": "FOCUS_STEPPER",
    "FOCUS_DRIVER_TYPE": "FOCUS_DRIVER",
    "DISPLAY_TYPE": "DISPLAY",
}


def shorten(string):
    return SHORT_STRINGS[string] if string in SHORT_STRINGS else string


# Define all possible parameters (boards and flags)
def create_problem():
    problem = Problem()
    problem.addVariable("BOARD", BOARDS)
    for key, values in BUILD_FLAGS.items():
        problem.addVariable(key, values)
    return problem


# Set constraints to the problem based on supported features
def set_support_constraints(problem):
    def board_supports_flag_value(b, k, v):
        return v in BOARD_SUPPORT[b][k]

    def board_support_constraint(expected_board, expected_flag):
        return lambda b, v: expected_board != b or board_supports_flag_value(b, expected_flag, v)

    # Apply board-feature support constraints
    for board, flags in BOARD_SUPPORT.items():
        for key, values in flags.items():
            constraint = board_support_constraint(board, key)
            problem.addConstraint(constraint, ["BOARD", key])

    # Apply stepper-driver support constraints
    def driver_supports_stepper(d, s):
        return d in STEPPER_SUPPORT[s]

    problem.addConstraint(driver_supports_stepper, ["RA_DRIVER_TYPE", "RA_STEPPER_TYPE"])
    problem.addConstraint(driver_supports_stepper, ["DEC_DRIVER_TYPE", "DEC_STEPPER_TYPE"])
    problem.addConstraint(driver_supports_stepper, ["ALT_DRIVER_TYPE", "ALT_STEPPER_TYPE"])
    problem.addConstraint(driver_supports_stepper, ["AZ_DRIVER_TYPE", "AZ_STEPPER_TYPE"])
    problem.addConstraint(driver_supports_stepper, ["FOCUS_DRIVER_TYPE", "FOCUS_STEPPER_TYPE"])


# Define constraints for excluded tests
def set_test_constraints(problem):
    # Reduce amount of boards under test
    # problem.addConstraint(InSetConstraint({"mega2560", "esp32", "mksgenlv21"}), ["BOARD"])

    problem.addConstraint(AllEqualConstraint(), [
        "RA_STEPPER_TYPE",
        "DEC_STEPPER_TYPE",
    ])

    problem.addConstraint(AllEqualConstraint(), [
        "ALT_STEPPER_TYPE",
        "AZ_STEPPER_TYPE",
    ])

    problem.addConstraint(AllEqualConstraint(), [
        "RA_DRIVER_TYPE",
        "DEC_DRIVER_TYPE",
    ])

    problem.addConstraint(AllEqualConstraint(), [
        "ALT_DRIVER_TYPE",
        "AZ_DRIVER_TYPE",
    ])


def set_ci_constraints(problem):
    problem.addConstraint(InSetConstraint({"DISPLAY_TYPE_NONE", "DISPLAY_TYPE_LCD_KEYPAD"}), ["DISPLAY_TYPE"])
    # problem.addConstraint(InSetConstraint({"DRIVER_TYPE_ULN2003"}), ["ALT_DRIVER_TYPE"])


def print_solutions_matrix(solutions, short_strings=False):
    def get_value(vb, vk):
        matching_solutions = list(filter(lambda sol: sol["BOARD"] == vb, solutions))
        values = set(map(lambda s: s[vk], matching_solutions))
        if short_strings:
            str_values = {"ALL"} if vk != "BOARD" and values == set(BUILD_FLAGS[vk]) else set(map(shorten, values))
        else:
            str_values = set(map(shorten, values))
        return "\n".join(str_values)

    boards = sorted(list(set(map(lambda s: s["BOARD"], solutions))))
    keys = list(solutions[0].keys())
    rows = [[get_value(board, key) for board in boards] for key in keys]

    print(tabulate.tabulate(rows, tablefmt="grid", showindex=map(shorten, keys), colalign=("right",)))


def print_failed_executor(executor: Executor):
    print(f'Error for the following configuration ({executor.proj_dir}):', file=sys.stderr)
    print_solutions_matrix([executor.solution])
    configuration_path = Path(executor.proj_dir, 'Configuration_local_matrix.hpp')
    print(f'{configuration_path}:')
    with open(configuration_path, 'r') as fp:
        print(fp.read())
    out_bytes, err_bytes = executor.proc.communicate()
    if out_bytes:
        print(out_bytes.decode())
    if err_bytes:
        print(err_bytes.decode(), file=sys.stderr)


def run_solution_blocking(executor: Executor, solution: dict) -> int:
    executor.solution = copy.deepcopy(solution)
    board = solution.pop("BOARD")
    executor.proc = execute(executor.proj_dir, board, solution, jobs=os.cpu_count(), out_pipe=False)
    executor.proc.wait()
    if executor.proc.returncode != 0:
        print_failed_executor(executor)
    return executor.proc.returncode


class GracefulKiller:
    kill_now = False

    def __init__(self):
        signal.signal(signal.SIGINT, self.exit_gracefully)
        signal.signal(signal.SIGTERM, self.exit_gracefully)

    def exit_gracefully(self):
        shutil.rmtree('.pio/build/matrix')
        self.kill_now = True


@click.command()
@click.option(
    '--board',
    '-b',
    type=click.Choice(BOARDS, case_sensitive=False),
    multiple=True,
    help="Limit boards under test. Multiple values allowed.")
def solve(board):
    # noinspection PyUnusedLocal
    killer = GracefulKiller()

    problem = create_problem()
    set_support_constraints(problem)

    if board:
        problem.addConstraint(InSetConstraint(board), ["BOARD"])

    set_test_constraints(problem)
    set_ci_constraints(problem)

    solutions = problem.getSolutions()
    print_solutions_matrix(solutions, short_strings=False)

    total_solutions = len(solutions)
    print(f'Testing {total_solutions} combinations')

    nproc = min(os.cpu_count(), len(solutions))

    local_paths_to_link = get_source_files_to_link()
    executor_list: List[Executor] = create_executors(nproc, local_paths_to_link)

    print('First run to fill cache')
    solution = solutions.pop()
    retcode = run_solution_blocking(executor_list[0], solution)
    if retcode != 0 and not CONTINUE_ON_ERROR:
        exit(retcode)

    copy_caches_to_executors(executor_list[0].proj_dir, executor_list[1:])

    solutions_built = 2  # We've already built one solution, and we're 1-indexing
    exit_early = False  # Exit trigger
    while solutions:
        # First fill any open execution slots
        while get_available_executor_idx(executor_list) is not None:
            available_executor_idx = get_available_executor_idx(executor_list)
            executor = executor_list[available_executor_idx]
            try:
                solution = solutions.pop()
            except IndexError:
                # No more solutions to try!
                break
            print(f'[{solutions_built}/{total_solutions}] Building ...')
            executor.solution = copy.deepcopy(solution)
            board = solution.pop("BOARD")
            executor.proc = execute(executor.proj_dir, board, solution)
            solutions_built += 1

        # Next wait for any processes to finish
        wait_for_executor_to_finish(executor_list)

        # Go through all the finished processes and check their status
        while get_finished_executor_idx(executor_list) is not None:
            finished_executor_idx = get_finished_executor_idx(executor_list)
            executor = executor_list[finished_executor_idx]
            if executor.proc.returncode != 0:
                print_failed_executor(executor)
                if not CONTINUE_ON_ERROR:
                    exit_early = True
            del executor.proc
            executor.proc = None

        if exit_early:
            break
    if exit_early:
        exit(1)
    print('Done!')
    cleanup_tempdirs(executor_list)


if __name__ == '__main__':
    solve()
