import subprocess
import sys

def build_proj():
    """
    build_proj() - A function to build the Zephyr project for nrf52833DK using west build command.
     It sources the necessary environment setup scripts and then runs the west build 
     command with appropriate parameters.
     If the build process fails, it catches the error and prints an error message before 
     exiting the script with a non-zero status code. If the build is successful, it prints a success message.
    """

    build_cmds = (
        "source /home/ethan/nrf-dev/ncs/zephyr/zephyr-env.sh && "
        "source /home/ethan/.venv/bin/activate && "
        "west build -p always -b nrf52833dk/nrf52833"
    )

    print("Building the project...")
    print("Running command: ", build_cmds)

    try:
        subprocess.run(build_cmds, shell=True, check=True, executable='/bin/bash')
        print("Build completed successfully.")
    except subprocess.CalledProcessError as e:
        print("Build failed with error: ", e)
        sys.exit(1)

if __name__ == "__main__":
    build_proj()