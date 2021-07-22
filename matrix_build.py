#!/usr/bin/env python3

import copy
import os
import shutil
import signal
import subprocess
import click

import tabulate
from constraint import *

CONTINUE_ON_ERROR = False

BOARDS = [
    "mega2560",
    "mksgenlv21",
    "mksgenlv2",
    "mksgenlv1",
    "esp32",
]

STEPPER_TYPES = [
    "STEPPER_TYPE_NONE",
    "STEPPER_TYPE_28BYJ48",
    "STEPPER_TYPE_NEMA17",
]

DRIVER_TYPES = [
    "DRIVER_TYPE_NONE",
    "DRIVER_TYPE_ULN2003",
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
    "DISPLAY_TYPE": DISPLAY_TYPES,
    "RA_MOTOR_CURRENT_RATING": "1",
    "RA_OPERATING_CURRENT_SETTING": "1",
    "DEC_MOTOR_CURRENT_RATING": "1",
    "DEC_OPERATING_CURRENT_SETTING": "1",
    "ALT_MOTOR_CURRENT_RATING": "1",
    "ALT_OPERATING_CURRENT_SETTING": "1",
    "AZ_MOTOR_CURRENT_RATING": "1",
    "AZ_OPERATING_CURRENT_SETTING": "1",
}

STEPPER_SUPPORT = {
    "STEPPER_TYPE_NONE": {
        "DRIVER_TYPE_NONE"
    },
    "STEPPER_TYPE_28BYJ48": {
        "DRIVER_TYPE_ULN2003"
    },
    "STEPPER_TYPE_NEMA17": {
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
    "mega2560": update_dict(BUILD_FLAGS, {
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
    }),
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
    }),
}

SHORT_STRINGS = {
    0: "DISABLED",
    1: "ENABLED",
    "STEPPER_TYPE_NONE": "NONE",
    "STEPPER_TYPE_28BYJ48": "28BYJ48",
    "STEPPER_TYPE_NEMA17": "NEMA17",
    "DRIVER_TYPE_NONE": "NONE",
    "DRIVER_TYPE_ULN2003": "ULN2003",
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


def generate_config_file(flag_values):
    content = "#pragma once\n\n"
    for key, value in flag_values.items():
        content += "#define {} {}\n".format(key, value)

    with open("Configuration_local_matrix.hpp", 'w') as f:
        f.write(content)
        print("Generated local config")
        print("Path: {}".format(os.path.abspath(f.name)))
        print("Content:")
        print(content)


def create_run_environment(flag_values):
    build_env = dict(os.environ)
    build_flags = " ".join(["-D{}={}".format(key, value) for key, value in flag_values.items()])
    build_env["PLATFORMIO_BUILD_FLAGS"] = build_flags
    return build_env


def execute(board, flag_values, use_config_file=True):
    if use_config_file:
        build_env = dict(os.environ)
        build_env["PLATFORMIO_BUILD_FLAGS"] = "-DMATRIX_LOCAL_CONFIG=1"
        generate_config_file(flag_values)
    else:
        build_env = create_run_environment(flag_values)

    proc = subprocess.Popen(
        "pio run -e {}".format(board),
        # stdout=subprocess.PIPE,
        # stderr=subprocess.PIPE,
        shell=True,
        env=build_env,
    )
    (stdout, stderr) = proc.communicate()
    return stdout, stdout, proc.returncode


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

    print("Testing {} combinations".format(len(solutions)))

    for num, solution in enumerate(solutions, start=1):
        print("[{}/{}] Building ...".format(num, len(solutions)), flush=True)
        print_solutions_matrix([solution])

        board = solution.pop("BOARD")
        (o, e, c) = execute(board, solution)
        if c and not CONTINUE_ON_ERROR:
            exit(c)
        print(flush=True)


if __name__ == '__main__':
    solve()
