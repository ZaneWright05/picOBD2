import os
import subprocess

print("Available drives:")
print("Please select the SD card drive, please ensure it is the correct drive as all data on it will be erased.")
drives = os.listdrives()
count = 0
for drive in drives:
    print(f"[{count}]: {drive}")
    count += 1

sdDrive = input("Enter the number of the drive you want to use: ")
try:
    if(int(sdDrive) >= 0 and int(sdDrive) < len(drives)): # bound check
        chosenDrive = drives[int(sdDrive)] # get the drive by index
        print(f"You have chosen: {chosenDrive}")
        confirm = input("Is this correct? (y/n): ")
        if confirm.lower() == 'y':
            print(f"Formatting {chosenDrive} as FAT32...")
            drive_letter = chosenDrive.replace("\\", "").replace(":", "")
            format_cmd = f'format {drive_letter}: /FS:FAT32 /Q /Y'
            print("You may be prompted for admin rights.")
            subprocess.run(format_cmd, shell=True)
            config_path = os.path.join(chosenDrive, "config.txt")
            with open(config_path, "wb") as f:
                name = input("Enter your username (default user, max 8 chars):") or "<user>"
                if len(name) > 8:
                    name = name[:8]
                f.write(f"username={name}\n".encode())
                maxCars = input("Enter the maximum number of cars (default 4, max 16):") or "4"
                if not maxCars.isdigit() or int(maxCars) < 1 or int(maxCars) > 16:
                    maxCars = "4"
                f.write(f"maxCars={maxCars}\n".encode())
                f.write(f"currentCars=0\n".encode())
                for i in range(int(maxCars)):
                    f.write(f"car{i+1}=\n".encode())
            print("SD card formatted and config.txt written.")
        else:
            print("Operation cancelled.")
            exit(0)
    else:
        print("Invalid drive number selected.")
        exit(1)
except ValueError:
    print("Invalid input, please enter a number corresponding to the drive.")
    exit(1)