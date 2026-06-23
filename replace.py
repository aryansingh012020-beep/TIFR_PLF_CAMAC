import os

def replace_in_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # control.c specifics
    content = content.replace('write(DrvFd,', 'camac_write(DrvFd,')
    content = content.replace('read(DrvFd,', 'camac_read(DrvFd,')
    content = content.replace('ioctl(DrvFd,CMC_CTL_RESET)', 'camac_reset(DrvFd)')
    content = content.replace('ioctl(DrvFd,CMC_CTL_R1)', 'camac_status(DrvFd)')
    content = content.replace('open("/dev/cmcamac0",O_RDWR)', 'camac_open("/dev/cmcamac0")')
    content = content.replace('close(DrvFd)', 'camac_close(DrvFd)')
    
    # cmc100test.c specifics
    content = content.replace('write(Fd,', 'camac_write(Fd,')
    content = content.replace('read(Fd,', 'camac_read(Fd,')
    content = content.replace('ioctl(Fd,CMC_CTL_RESET)', 'camac_reset(Fd)')
    content = content.replace('ioctl(Fd,CMC_CTL_R1)', 'camac_status(Fd)')
    content = content.replace('open(Name,O_RDWR)', 'camac_open(Name)')
    content = content.replace('close(Fd)', 'camac_close(Fd)')

    with open(filepath, 'w') as f:
        f.write(content)

replace_in_file('src/control.c')
replace_in_file('src/cmc100test.c')

print("Replacements done.")
