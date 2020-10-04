/****************************************************************************************************
 *  Sort lexicographically lines in file with limited mmory.
 *      This progrma runs under Linux OS to ensure it can be use some supported libraries.
 *      This program has not been tested under other OS yet.
 *---------------------------------------------------------------------------------------------------
 *  Steps of algorithm:
 *  - Split the input file to slices whose text lines can be stored in allocated memory area.
 *  - Sort in increase-order for each slices, then write each result slice to a temporary file.
 *      (All temporary file names will have index number to use loop later)
 *  - Loop until all temporary files don't remain any line to read:
 *      +  Read the 1st line of the 1st temporary file, mark it as min string.
 *      +  Loop through all other temporary files:
 *          . Read the 1st line of each temporary file,
 *          . Compare this line to the current min string to select the new min string.
 *          . After read 1st line of the last temporary file, we have the current min string.
 *          . Write the current min string to the output file.
 *          . Discard the current min string out of its temporary file.
 *---------------------------------------------------------------------------------------------------
 *  Command to compile this program:
 *      g++ main.cpp -o execute_file_name
 ****************************************************************************************************/

#include <iostream>
#include <ios>
#include <fstream>          // for file stream processing
#include <string>           // for string class and related functions
#include <vector>           // for vector class and related functions
#include <algorithm>        // for sort()
#include <sys/stat.h>       // use stat() and its related struct to check existing
#include <unistd.h>         // for system constants


using namespace std;


/* Constants */
#define TEMP_PREFIX     "temp_"


/* Global variables */
unsigned long long gMemLimit;


/* Prototypes */
void memUsage(double &dVmUsage, double &dResidentSet);
bool fileExisting(const char* szFilename, unsigned long long &lFilesize);
unsigned long splitInput(const char* szFilename, unsigned long long &lTotalLines);
void writeOutput(const char* szFilename, unsigned long iTotalFiles, unsigned long long lTotalLines);
void sortWriteSlice(vector<string> vLines, unsigned long iFilenumber);
unsigned long long fileSize(const char* szFilename);
void discard1stLine(const char* szFilename);


/********************************************************************************
 *  Main function
 *--------------------
 *  Starting point of program
 *--------------------
 *  Params:
 *      argc    [I]     Number of arguments which user types
 *      argv    [I]     Array of arguments which use types
 *--------------------
 *  Return:
 *      0: program run successfully.
 *      1: invalid arguments
 *     -1: other errors.
 ********************************************************************************/
int main(int argc, char* argv[])
{
    // Check arguments
    if (argc < 4)
    {
        cout << "\n    Usage:  " << argv[0] << " <in_filename> <out_filename> <mem_limit>\n\n";
        return 1;
    }

    unsigned long long lFilesize = 0;

    // Check whether input file is existing or not, and also get file size
    if (fileExisting(argv[1], lFilesize) != true)
    {
        cout << "\n The input file (" << argv[1] << ") is not existing!\n\n";
        return -1;
    }

    // Get memory limitation, anh check the minimum allowance
    gMemLimit = stoll(argv[3], NULL, 10);   // get amount from argument
    double dVm, dRss;
    memUsage(dVm, dRss);                    // get occupancy of process
    if (gMemLimit <= (unsigned long long)(dVm + dRss))
    {
        cout << "\n The memory limitation is so small! It should be at least " << (unsigned long long)(dVm + dRss) +1 << " Bytes.\n\n";
        return -1;
    }

    // Split the input file to temporary file which have sorted lines
    unsigned long iTotalTempFiles = 0;
    unsigned long long lTotalLines = 0;
    iTotalTempFiles = splitInput(argv[1], lTotalLines);

    // Write each line from temporary files to the output file
    writeOutput(argv[2], iTotalTempFiles, lTotalLines);

    cout << "\n\n";
    return 0;
}


/********************************************************************************
 *  memUsage()
 *--------------------
 *  Get memory usage (Virtual Memory and Resident Set) of current process.
 *      These information is read from /proc/self/stat
 *--------------------
 *  Params:
 *      dVmUsage        [I]     Virtual Memory in Bytes
 *      dResidentSet    [O]     Resident Set in Bytes
 *--------------------
 *  Return: N/A
 ********************************************************************************/
void memUsage(double &dVmUsage, double &dResidentSet)
{
    dVmUsage = 0.0;
    dResidentSet = 0.0;

    // Open /proc/self/stat
    ifstream stat_stream("/proc/self/stat", ios_base::in);

    //Variables to get info
    string pid, comm, state, ppid, pgrp, session, tty_nr;
    string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    string utime, stime, cutime, cstime, priority, nice;
    string O, itrealvalue, starttime;
    unsigned long vsize, rss;

    // Read values
    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
    >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
    >> utime >> stime >> cutime >> cstime >> priority >> nice
    >> O >> itrealvalue >> starttime >> vsize >> rss;

    stat_stream.close();        // close /proc/self/stat

    // Convert VM to Bytes
    dVmUsage = vsize / 1024.0;

    // Convert RSS to Bytes
    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
    dResidentSet = rss * page_size_kb;
}


/********************************************************************************
 *  fileExisting()
 *--------------------
 *  Check whether the specified file is existing or not
 *--------------------
 *  Params:
 *      szFilename      [I]     String of file name
 *      lFilesize       [O]     File size in Bytes
 *--------------------
 *  Return:
 *      bool
 *          true: file is existing
 *          false: file is not existing
 ********************************************************************************/
bool fileExisting(const char* szFilename, unsigned long long &lFilesize)
{
    struct stat stFilestat;

    if (stat(szFilename, &stFilestat) == 0)
    {
        lFilesize = stFilestat.st_size;
        return true;
    }
    else
    {
        return false;
    }
}


/********************************************************************************
 *  splitInput()
 *--------------------
 *  Split the input file to slices,
 *  sort in increase-order for lines of each slice,
 *  then write the result to temporary files.
 *--------------------
 *  Params:
 *      szFilename      [I]     Name of the input file
 *--------------------
 *  Return:
 *      unsigned long: Number of temporary files.
 ********************************************************************************/
unsigned long splitInput(const char* szFilename, unsigned long long &lTotalLines)
{
    unsigned long    iTotalFiles = 0;

    fstream     stInputfile;
    stInputfile.open(szFilename, ios::in);

    string      szTempFilename;
    fstream     stTempfile;

    double dVm, dRss;
    unsigned long long  lCurrentSize = 0;
    vector<string>  vLines;
    string          szCurrentLine;

    lTotalLines = 0;

    // Loop through all lines in the input file
    while (!stInputfile.eof())
    {
        getline(stInputfile, szCurrentLine);

        memUsage(dVm, dRss);            // get current occupancy of process

        if (((unsigned long long)(dVm + dRss) + lCurrentSize + sizeof(string) + szCurrentLine.size()) <= gMemLimit)
        {
            if (szCurrentLine.size() > 0)           // only store line which actually has content
            {
                vLines.push_back(szCurrentLine);
                lTotalLines += 1;

                lCurrentSize += (sizeof(string) + szCurrentLine.size());
            }
        }
        else        // This means current lines exceed size of allowed memory, so sort then write these lines to temporary file
        {
            // Sort then write the current lines to temporary file
            sortWriteSlice(vLines, iTotalFiles);

            // Set value for controlling variables
            iTotalFiles += 1;
            lCurrentSize = 0;
            vLines.clear();

            // Move file-pointer backward to the starting of current line in file, because current line has not process yet
            stInputfile.seekg(-(szCurrentLine.size() + 1), ios_base::cur);
        }
    }

    // After loop through all lines of the input file, if it remains a list of lines, write that list to a temporary file.
    if (vLines.size() > 0)
    {
        sortWriteSlice(vLines, iTotalFiles);

        iTotalFiles += 1;

        vLines.clear();         // clear data in vector
    }

    // Close the input file
    stInputfile.close();

    return iTotalFiles;
}


/********************************************************************************
 *  writeOutput()
 *--------------------
 *  Write each line from all temporary files to the output file.
 *--------------------
 *  Params:
 *      szFilename      [I]     Name of the output file
 *      iTotalFiles     [I]     Total number of temporary files
 *      fTotalLines     [I]     Total number of lines in all temporary files
 *--------------------
 *  Return: N/A
 ********************************************************************************/
void writeOutput(const char* szFilename, unsigned long iTotalFiles, unsigned long long lTotalLines)
{
    fstream stOutputfile;
    stOutputfile.open(szFilename, ios::out);        // open the output file to write (append)

    unsigned long long lWrittenLines = 0;

    string      szTempFilename, szMinFilename;
    fstream     stTempfile;

    unsigned long       arFilenumber[2];
    string              arLines[2];
    unsigned long long lMinIdx, lOtherIdx, lTempIdx;

    // Prepare for the main loop: Read 1st line of the 1st temporary file, and mark it as current min string
    lMinIdx = 0;
    lOtherIdx = 1;
    szTempFilename = string(TEMP_PREFIX) + "0";
    stTempfile.open(szTempFilename, ios::in);
    getline(stTempfile, arLines[lMinIdx]);
    stTempfile.close();
    arFilenumber[lMinIdx] = 0;
    szMinFilename = szTempFilename;

    // Loop through all lines in all temporary files
    while (lWrittenLines < lTotalLines)
    {
        // Loop through other temporary files to read 1st lines and compare with the current min string
        for (unsigned long iFilenumber = 0; iFilenumber < iTotalFiles; iFilenumber++)
        {
            if (iFilenumber == arFilenumber[lMinIdx])       // If current temporary file is the file has current min string, skip to next temporary file
            {
                continue;
            }

            // Read 1st line of current temporary file
            szTempFilename = string(TEMP_PREFIX) + to_string(iFilenumber);
            if (fileSize(szTempFilename.c_str()) <= 0)          // If current temporary file size is 0, skip to next temporary file
            {
                continue;
            }

            stTempfile.open(szTempFilename, ios::in);
            getline(stTempfile, arLines[lOtherIdx]);
            stTempfile.close();
            arFilenumber[lOtherIdx] = iFilenumber;

            // If this line is less than the current min string, mark it is current min string
            if (arLines[lOtherIdx] < arLines[lMinIdx])
            {
                lTempIdx = lMinIdx;
                lMinIdx = lOtherIdx;
                lOtherIdx = lTempIdx;

                szMinFilename = szTempFilename;
            }
        }

        // Write (append) current min string to the output file
        stOutputfile.write((arLines[lMinIdx] + "\n").c_str(), arLines[lMinIdx].size() + 1);

        // Increase number of written lines
        lWrittenLines += 1;

        // Discard 1st line out of its temporary file
        discard1stLine(szMinFilename.c_str());

        // Because the min string was written, the other one in array is marked as the new min string
        lTempIdx = lMinIdx;
        lMinIdx = lOtherIdx;
        lOtherIdx = lTempIdx;

        // Create szMinFilename for next while loop
        szMinFilename = string(TEMP_PREFIX) + to_string(arFilenumber[lMinIdx]);
    }

    stOutputfile.close();

    cout << "\n  Sorting is finished.\n    Total number of lines: " << lTotalLines;

    // Remove all temporary file
    system((string("rm ") + TEMP_PREFIX + "*").c_str());
}


/********************************************************************************
 *  sortWriteSlice()
 *--------------------
 *  Sort then write slice (list of lines) to a temporary file
 *--------------------
 *  Params:
 *      vLines          [I]     List of lines
 *      iFilenumber     [I]     Order number of temporary file
 *--------------------
 *  Return: N/A
 ********************************************************************************/
void sortWriteSlice(vector<string> vLines, unsigned long iFilenumber)
{
    vector<string>::iterator ite, ite2;
    string      szTempFilename;
    fstream     stTempfile;

    // Sort lines of this slice
    sort(vLines.begin(), vLines.end());

    // Write all lines of current slice to temporary file
    szTempFilename = string(TEMP_PREFIX) + to_string(iFilenumber);
    stTempfile.open(szTempFilename, ios::out);

    ite2 = vLines.end();
    for (ite = vLines.begin(); ite != ite2; ite++)
    {
        stTempfile.write((*ite + "\n").c_str(), (*ite).size() + 1);
    }

    stTempfile.close();
}


/********************************************************************************
 *  fileSize()
 *--------------------
 *  Get size of a file (in Bytes)
 *--------------------
 *  Params:
 *      szFilename      [I]     Name of the file to get size
 *--------------------
 *  Return:
 *      unsigned long long: File size in Bytes
 ********************************************************************************/
unsigned long long fileSize(const char* szFilename)
{
    struct stat stFilestat;
    
    if (stat(szFilename, &stFilestat) == 0)
    {
        return stFilestat.st_size;
    }
    else
    {
        return -1;
    }
}


/********************************************************************************
 *  discard1stLine()
 *--------------------
 *  Discard 1st line of a file:
 *      Copy from 2nd line to a new file, then remove the original file.
 *--------------------
 *  Params:
 *      szFilename      [I]     Name of the file to be discarded 1st line
 *--------------------
 *  Return: N/A
 ********************************************************************************/
void discard1stLine(const char* szFilename)
{
    fstream     stOrgFile;
    fstream     stNewFile;

    stOrgFile.open(szFilename, ios::in);

    string      szNewFilename = string("new_") + szFilename;
    stNewFile.open(szNewFilename, ios::out);

    string szLine;
    getline(stOrgFile, szLine);     // read (to skip) 1st line of the original file
    // Read from 2nd line and write to new file
    while (!stOrgFile.eof())
    {
        getline(stOrgFile, szLine);
        if (szLine.size() > 0)
        {
            stNewFile.write((szLine + "\n").c_str(), szLine.size() + 1);
        }
    }

    stNewFile.close();
    stOrgFile.close();

    // Remove the original file, and rename the new file to original file name
    remove(szFilename);
    rename(szNewFilename.c_str(), szFilename);
}

