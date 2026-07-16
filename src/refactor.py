import re

def refactor_file(filepath, fd_name):
    with open(filepath, 'r') as f:
        content = f.read()

    # Remove CamacNAF definition
    content = re.sub(r'g?int CamacNAF\(gint Data,gint N,gint A,gint F,g[a-z]* \*(XRes|X),g[a-z]* \*(QRes|Q),g[a-z]* \*(Lam|L)\)[\s\S]*?\{[\s\S]*?\}', '', content)
    # Remove FullCamacNAF definition
    content = re.sub(r'int FullCamacNAF\(gint Data,gint N,gint A,gint F,gboolean \*XRes,gboolean \*QRes,gboolean \*Lam\)[\s\S]*?\{[\s\S]*?\}', '', content)

    # Replace calls
    content = re.sub(r'\bCamacNAF\(', f'camac_naf({fd_name}, ', content)
    content = re.sub(r'\bFullCamacNAF\(', f'camac_naf_full({fd_name}, ', content)

    # ioctl replacements
    content = content.replace(f'ioctl({fd_name}, CMC_CTL_RESET)', f'camac_reset({fd_name})')
    content = content.replace(f'ioctl({fd_name}, CMC_CTL_R1)', f'camac_status({fd_name})')
    content = content.replace(f'ioctl({fd_name},IOC_VTRANSPD_WRITE1,NULL)', f'camac_stop_acqn({fd_name}, 1)')
    content = content.replace(f'ioctl({fd_name},IOC_VTRANSPD_WRITE2,NULL)', f'camac_stop_acqn({fd_name}, 2)')

    # LP replacements
    content = re.sub(r'XData=0xFFFFFFFF;\s*camac_write\([^,]+,&XData,4\);\s*XData=0x00000000;\s*camac_write\([^,]+,&XData,4\);', f'camac_lp_header({fd_name});', content)
    content = re.sub(r'D=0xFFFFFFFF;\s*camac_write\([^,]+,&D,4\);\s*D=0x00000000;\s*camac_write\([^,]+,&D,4\);', f'camac_lp_header({fd_name});', content)

    content = re.sub(r'XData=\(3<<24\) \| ProgramLocation\+\+;\s*camac_write\([^,]+,&XData,4\);', f'camac_lp_store_next({fd_name}, ProgramLocation++);', content)
    content = re.sub(r'XData=\(3<<24\) \| ProgramLocation;\s*camac_write\([^,]+,&XData,4\);', f'camac_lp_store_next({fd_name}, ProgramLocation);', content)

    content = re.sub(r'XData=A\|F<<4\|N<<9;\s*camac_write\([^,]+,&XData,4\);', f'camac_lp_naf({fd_name}, N, A, F);', content)

    content = re.sub(r'XData=\(5<<24\)\| 3;\s*camac_write\([^,]+,&XData,4\);', f'camac_lp_delay({fd_name}, 3);', content)
    
    content = re.sub(r'XData=\(12<<24\)\| ([A-Z_a-z0-9]+);\s*camac_write\([^,]+,&XData,4\);', rf'camac_lp_literal({fd_name}, \1);', content)
    content = re.sub(r'D=\(12<<24\)\|Data;\s*camac_write\([^,]+,&D,4\);', f'camac_lp_literal({fd_name}, Data);', content)

    content = re.sub(r'XData=\(2<<24\) \| \(1<<23\) \| 15;\s*camac_write\([^,]+,&XData,4\);', f'camac_lp_repeat({fd_name}, 1, 15);', content)

    content = re.sub(r'XData=\(31<<24\);\s*camac_write\([^,]+,&XData,4\);', f'camac_lp_quit({fd_name});', content)
    content = re.sub(r'D=0x0E000000;\s*camac_write\([^,]+,&D,4\);', f'camac_flush({fd_name});', content)
    content = re.sub(r'XData=0x0E000000;\s*camac_write\([^,]+,&XData,4\);', f'camac_flush({fd_name});', content)

    # specific to control.c
    content = content.replace('camac_write(DrvFd,&Code,sizeof(short));', f'camac_write_short({fd_name}, Code);')
    content = content.replace('camac_write(Fd,&Cmd[i],sizeof(short));', f'camac_write_short(Fd, Cmd[i]);')
    content = content.replace('camac_write(Fd,&Arg[i],sizeof(short));', f'camac_write_short(Fd, Arg[i]);')

    with open(filepath, 'w') as f:
        f.write(content)

refactor_file('control.c', 'DrvFd')
refactor_file('cmc100test.c', 'Fd')
