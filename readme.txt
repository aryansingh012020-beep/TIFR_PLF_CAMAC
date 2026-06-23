             LAMPS_CMC100 Data Acquisition with the CMC100 Controller

You will need to know the root password if you are installing the driver.

Decide on a user account and create a directory for the installation.
Lets say the user account name is cmc100 and the directory is lamps_cmc100.
The entire package, including the driver is in a single tar archive. 
Download and copy lamps-cmc100-dd-mm-yy.tgz (the version date would change) in this directory
Change to this directory.
-----------------------------------------------------------------------------------------------------
Before you start (IMPORTANT STEP):
Edit your .bash_profile file (a hidden file in your home directory)
$vi ~/.bash_profile
Change the line PATH=$PATH:$HOME/bin to PATH=$PATH:$HOME/bin:./
Logout and then login again so as to activate the new profile
-----------------------------------------------------------------------------------------------------
Unpacking:
Execute tar zxvf lamps-cmc100-dd-mm-yy.tgz
-----------------------------------------------------------------------------------------------------
Compile lamps:
$cd src (go the directory src where the source code is stored)
$make   (compile the program. A number of executables are generated and kept in the upper level directory)
$cd ..  (come back to the upper level directory)
-----------------------------------------------------------------------------------------------------
Compile the driver:
Skip this step if you are interested in offline analysis only
$cd driver (go the directory diver where the driver source code is stored)
$BuildDriver  (compile the driver)
-----------------------------------------------------------------------------------------------------
Install the driver:
Skip this step if you are interested in offline analysis only
$su             (become root)
password:       (enter the root password)
#vi /etc/passwd (edit the file /etc/passwd)
Add the following line at the end of the file:
cmc::0:0:root:/home/cmc:/home/cmc/ldcmc100
Save and exit the editor
#mkdir /home/cmc                         (make a new directory)
#cd /home/cmc                            (go to this directory)
#cp /home/cmc100/lamps_cmc100/ldcmc100 . (copy file from installation directory)
-----------------------------------------------------------------------------------------------------
Running:
$cd /home/cmc100/lamps_cmc100  (go the directory where you installed lamps)
$lamps                         (because of the PATH in .bash_profile you dont need to type ./lamps)
The first time you run you will see the LAMPS Preferences window. Click "Save and Exit" 
-----------------------------------------------------------------------------------------------------
Hardware:
1. Set the controller address to 0 (zero) by means of the rotatry knob on the front panel
   of the CMC100 y means of a small screwdriver
2. Insert the CMC100 in the right-most position of the CAMAC crate
3. Connect a USB cable from the CMC100 to PC
4. Switch on the crate
5. Type dmesg. You should be able to see the CMC100 device
-----------------------------------------------------------------------------------------------------
Acquisition:
Although the root password was required while installing the driver, it is not needed any more.
1. From the LAMPS main window, click Acquisition->LOAD DRIVER
   It takes about 5 sec for the driver to load. If you swich off the crate at any time
   LOAD DRIVER will be required again after switching on.
2. Prepare a setup file (Setup->Edit Setup)
3. Aquisition->Start Acquisition
-----------------------------------------------------------------------------------------------------

