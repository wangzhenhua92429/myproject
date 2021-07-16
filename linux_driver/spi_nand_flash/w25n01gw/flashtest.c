#include <stdio.h>
#include <string.h>
#include <sys/types.h>    
#include <sys/stat.h>    
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <mtd/mtd-user.h>

#define SPI_FLASH_COLUMN_SIZE (512)
#define SPI_FLASH_PAGE_SIZE   (4 * SPI_FLASH_COLUMN_SIZE) // 2048B/page
#define SPI_FLASH_BLOCK_SIZE  (64 * SPI_FLASH_PAGE_SIZE)  //64page

struct band{
    unsigned int band1;
    unsigned int band2;
    unsigned int band3;
    unsigned int band4;
};

struct calibration{
    double          RadiometricCalibration1;
    int             RadiometricCalibration2;
    int             RadiometricCalibration3;

    unsigned int    DarkRowValue1;
    unsigned int    DarkRowValue2;
    unsigned int    DarkRowValue3;
    unsigned int    DarkRowValue4;

    float           VignettingCenter1;
    float           VignettingCenter2;

    double          VignettingPolynomial0;
    double          VignettingPolynomial1;
    double          VignettingPolynomial2;
    double          VignettingPolynomial3;

    double          PerspectiveFocalLength;

    double          PerspectiveDistortion1;
    double          PerspectiveDistortion2;
    double          PerspectiveDistortion3;
    double          PerspectiveDistortion4;
    double          PerspectiveDistortion5;

    int             BootTimeStamp;

    long            TimeStamp;

    double          BandSensitivity;

    double          RawMeasurement;

    double          OffMeasurement;

    int             BlackLevel;

    double          PrincipalPoint1;
    double          PrincipalPoint2;
    double          RelativeOpticalCenterX;
    double          RelativeOpticalCenterY;
};

struct message {
    struct band         stBand;
    struct calibration  stCalibration1;
    struct calibration  stCalibration2;
    struct calibration  stCalibration3;
    struct calibration  stCalibration4;
};

int load_ini_file(const char *file, char *buf, int *buf_size) {
	FILE *in = NULL;
	int i = 0;
	*buf_size = 0;

	in = fopen(file, "r");
	if (NULL == in) {
		return 0;
	}

	buf[i] = fgetc(in);

	//load initialization file
	while (buf[i] != (char) EOF) {
		i++;
		if (i >= (1024*9)) { //file too big, you can redefine MAX_FILE_SIZE to fit the big file
			return -1;
		}
		buf[i] = fgetc(in);
	}

	buf[i] = '\0';
	*buf_size = i;

	fclose(in);
	return 1;
}

#if 1
int main()
{
    int fd, fd1;
    struct erase_info_user erase;
    loff_t offset;
    struct message stMessage;
    char buf[SPI_FLASH_PAGE_SIZE * 3];
    int bufCfgLen;

    printf("sizeof = %d\n", sizeof(stMessage));

    stMessage.stBand.band1 = 555;
    stMessage.stBand.band2 = 660;
    stMessage.stBand.band3 = 720;
    stMessage.stBand.band4 = 840;

    stMessage.stCalibration1.RadiometricCalibration1 = 961928.80313;
    stMessage.stCalibration1.RadiometricCalibration2 = 1;
    stMessage.stCalibration1.RadiometricCalibration3 = 0;

    stMessage.stCalibration1.DarkRowValue1 = 2344;
    stMessage.stCalibration1.DarkRowValue2 = 2344;
    stMessage.stCalibration1.DarkRowValue3 = 2344;
    stMessage.stCalibration1.DarkRowValue4 = 2344;

    stMessage.stCalibration1.VignettingCenter1 = 567.983;
    stMessage.stCalibration1.VignettingCenter2 = 660.291;

    stMessage.stCalibration1.VignettingPolynomial0 = 1.03452;
    stMessage.stCalibration1.VignettingPolynomial1 = 0.000174829;
    stMessage.stCalibration1.VignettingPolynomial2 = -5.28386e-07;
    stMessage.stCalibration1.VignettingPolynomial3 = 1.12621e-09;

    stMessage.stCalibration1.PerspectiveFocalLength = 6.0000;

    stMessage.stCalibration1.PerspectiveDistortion1 = 0.00000;
    stMessage.stCalibration1.PerspectiveDistortion2 = 0.00000;
    stMessage.stCalibration1.PerspectiveDistortion3 = 0.00000;
    stMessage.stCalibration1.PerspectiveDistortion4 = 0.00000;
    stMessage.stCalibration1.PerspectiveDistortion5 = 0.00000;

    stMessage.stCalibration1.BootTimeStamp = 739;
    stMessage.stCalibration1.TimeStamp = 630959;
    stMessage.stCalibration1.BandSensitivity = 0.98180;
    stMessage.stCalibration1.RawMeasurement = 0.00000;
    stMessage.stCalibration1.OffMeasurement = 0.002;
    stMessage.stCalibration1.BlackLevel = 2344;
    stMessage.stCalibration1.PrincipalPoint1 = 1.458;
    stMessage.stCalibration1.PrincipalPoint2 = 1.728;
    stMessage.stCalibration1.RelativeOpticalCenterX = 0.00000;
    stMessage.stCalibration1.RelativeOpticalCenterY = 0.00000;

    //2
    stMessage.stCalibration2.RadiometricCalibration1 = 961928.80313;
    stMessage.stCalibration2.RadiometricCalibration2 = 1;
    stMessage.stCalibration2.RadiometricCalibration3 = 0;

    stMessage.stCalibration2.DarkRowValue1 = 2344;
    stMessage.stCalibration2.DarkRowValue2 = 2344;
    stMessage.stCalibration2.DarkRowValue3 = 2344;
    stMessage.stCalibration2.DarkRowValue4 = 2344;

    stMessage.stCalibration2.VignettingCenter1 = 567.983;
    stMessage.stCalibration2.VignettingCenter2 = 660.291;

    stMessage.stCalibration2.VignettingPolynomial0 = 1.03452;
    stMessage.stCalibration2.VignettingPolynomial1 = 0.000174829;
    stMessage.stCalibration2.VignettingPolynomial2 = -5.28386e-07;
    stMessage.stCalibration2.VignettingPolynomial3 = 1.12621e-09;

    stMessage.stCalibration2.PerspectiveFocalLength = 6.0000;

    stMessage.stCalibration2.PerspectiveDistortion1 = 0.00000;
    stMessage.stCalibration2.PerspectiveDistortion2 = 0.00000;
    stMessage.stCalibration2.PerspectiveDistortion3 = 0.00000;
    stMessage.stCalibration2.PerspectiveDistortion4 = 0.00000;
    stMessage.stCalibration2.PerspectiveDistortion5 = 0.00000;

    stMessage.stCalibration2.BootTimeStamp = 739;
    stMessage.stCalibration2.TimeStamp = 630959;
    stMessage.stCalibration2.BandSensitivity = 0.98180;
    stMessage.stCalibration2.RawMeasurement = 0.00000;
    stMessage.stCalibration2.OffMeasurement = 0.002;
    stMessage.stCalibration2.BlackLevel = 2344;
    stMessage.stCalibration2.PrincipalPoint1 = 1.458;
    stMessage.stCalibration2.PrincipalPoint2 = 1.728;
    stMessage.stCalibration2.RelativeOpticalCenterX = 0.00000;
    stMessage.stCalibration2.RelativeOpticalCenterY = 0.00000;

    //3
    stMessage.stCalibration3.RadiometricCalibration1 = 961928.80313;
    stMessage.stCalibration3.RadiometricCalibration2 = 1;
    stMessage.stCalibration3.RadiometricCalibration3 = 0;

    stMessage.stCalibration3.DarkRowValue1 = 2344;
    stMessage.stCalibration3.DarkRowValue2 = 2344;
    stMessage.stCalibration3.DarkRowValue3 = 2344;
    stMessage.stCalibration3.DarkRowValue4 = 2344;

    stMessage.stCalibration3.VignettingCenter1 = 567.983;
    stMessage.stCalibration3.VignettingCenter2 = 660.291;

    stMessage.stCalibration3.VignettingPolynomial0 = 1.03452;
    stMessage.stCalibration3.VignettingPolynomial1 = 0.000174829;
    stMessage.stCalibration3.VignettingPolynomial2 = -5.28386e-07;
    stMessage.stCalibration3.VignettingPolynomial3 = 1.12621e-09;

    stMessage.stCalibration3.PerspectiveFocalLength = 6.0000;

    stMessage.stCalibration3.PerspectiveDistortion1 = 0.00000;
    stMessage.stCalibration3.PerspectiveDistortion2 = 0.00000;
    stMessage.stCalibration3.PerspectiveDistortion3 = 0.00000;
    stMessage.stCalibration3.PerspectiveDistortion4 = 0.00000;
    stMessage.stCalibration3.PerspectiveDistortion5 = 0.00000;

    stMessage.stCalibration3.BootTimeStamp = 739;
    stMessage.stCalibration3.TimeStamp = 630959;
    stMessage.stCalibration3.BandSensitivity = 0.98180;
    stMessage.stCalibration3.RawMeasurement = 0.00000;
    stMessage.stCalibration3.OffMeasurement = 0.002;
    stMessage.stCalibration3.BlackLevel = 2344;
    stMessage.stCalibration3.PrincipalPoint1 = 1.458;
    stMessage.stCalibration3.PrincipalPoint2 = 1.728;
    stMessage.stCalibration3.RelativeOpticalCenterX = 0.00000;
    stMessage.stCalibration3.RelativeOpticalCenterY = 0.00000;

    //4
    stMessage.stCalibration4.RadiometricCalibration1 = 961928.80313;
    stMessage.stCalibration4.RadiometricCalibration2 = 1;
    stMessage.stCalibration4.RadiometricCalibration3 = 0;

    stMessage.stCalibration4.DarkRowValue1 = 2344;
    stMessage.stCalibration4.DarkRowValue2 = 2344;
    stMessage.stCalibration4.DarkRowValue3 = 2344;
    stMessage.stCalibration4.DarkRowValue4 = 2344;

    stMessage.stCalibration4.VignettingCenter1 = 567.983;
    stMessage.stCalibration4.VignettingCenter2 = 660.291;

    stMessage.stCalibration4.VignettingPolynomial0 = 1.03452;
    stMessage.stCalibration4.VignettingPolynomial1 = 0.000174829;
    stMessage.stCalibration4.VignettingPolynomial2 = -5.28386e-07;
    stMessage.stCalibration4.VignettingPolynomial3 = 1.12621e-09;

    stMessage.stCalibration4.PerspectiveFocalLength = 6.0000;

    stMessage.stCalibration4.PerspectiveDistortion1 = 0.00000;
    stMessage.stCalibration4.PerspectiveDistortion2 = 0.00000;
    stMessage.stCalibration4.PerspectiveDistortion3 = 0.00000;
    stMessage.stCalibration4.PerspectiveDistortion4 = 0.00000;
    stMessage.stCalibration4.PerspectiveDistortion5 = 0.00000;

    stMessage.stCalibration4.BootTimeStamp = 739;
    stMessage.stCalibration4.TimeStamp = 630959;
    stMessage.stCalibration4.BandSensitivity = 0.98180;
    stMessage.stCalibration4.RawMeasurement = 0.00000;
    stMessage.stCalibration4.OffMeasurement = 0.002;
    stMessage.stCalibration4.BlackLevel = 2344;
    stMessage.stCalibration4.PrincipalPoint1 = 1.458;
    stMessage.stCalibration4.PrincipalPoint2 = 1.728;
    stMessage.stCalibration4.RelativeOpticalCenterX = 0.00000;
    stMessage.stCalibration4.RelativeOpticalCenterY = 0.00000;

    fd = open("/dev/mtd3", O_SYNC | O_RDWR);
    if(fd < 0) {
        printf("file open failured!\n");
    }

    fd1 = open("/opt/dgp/app/calibrationDatatest.ini", O_SYNC | O_RDWR);
    if(fd < 0) {
        printf("calibrationDatatest file open failured!\n");
    }

#if 1
    //擦除
    erase.start = 0;
    erase.length = 1024 * SPI_FLASH_BLOCK_SIZE;

    if (ioctl(fd, MEMERASE, &erase) != 0) //执行擦除操作
    {
        //fprintf(stderr, "\n%s: MTD Erase failure: %s\n", DevInfo->dir,strerror(errno));
        printf("MTD Erase failure\n");
    }
#endif

#if 1
    offset = 0;
    lseek(fd, offset, SEEK_SET);

    memcpy(buf, &stMessage.stBand, sizeof(stMessage.stBand));

    load_ini_file("/opt/dgp/app/calibrationDatatest.ini",
        buf + sizeof(stMessage.stBand), &bufCfgLen);

    write(fd, buf, SPI_FLASH_PAGE_SIZE * 3);
#endif

#if 0
    memset(buf, 0x0, SPI_FLASH_PAGE_SIZE * 3);

    offset = 0;
    lseek(fd, offset, SEEK_SET);

    read(fd, buf, SPI_FLASH_PAGE_SIZE * 3);
    memcpy(&stMessage, buf, sizeof(stMessage));
    printf("band1 = %d.\n", stMessage.stBand.band1);
#endif
    return 0;
}
#else
int main()
{
    int fd;
    struct erase_info_user erase;
    loff_t offset;
    char buf[SPI_FLASH_PAGE_SIZE * 2];
    int i, j;

    fd = open("/dev/mtd3", O_SYNC | O_RDWR);
    if(fd < 0) {
        printf("file open failured!\n");
    }
#if 0
    //擦除
    erase.start = 0;
    erase.length = 1024 * SPI_FLASH_BLOCK_SIZE;

    if (ioctl(fd, MEMERASE, &erase) != 0) //执行擦除操作
    {
        //fprintf(stderr, "\n%s: MTD Erase failure: %s\n", DevInfo->dir,strerror(errno));
        printf("MTD Erase failure\n");
    }
#endif

#if 1
    //写入
    for(i = 0; i < 8;i++)
    {
        for(j = 0; j < 256; j++)
        {
            buf[i * 256 + j] = j;
        }
    }

    for(i = 0; i < 8;i++)
    {
        for(j = 0; j < 256; j++)
        {
            buf[SPI_FLASH_PAGE_SIZE + (i * 256 + j)] = j;
        }
    }

    offset = 0;
    lseek(fd, offset, SEEK_SET);

    write(fd, buf, SPI_FLASH_PAGE_SIZE * 2);
#endif

#if 1
    memset(buf, 0x0, SPI_FLASH_PAGE_SIZE * 2);

    offset = 0;
    lseek(fd, offset, SEEK_SET);

     read(fd, buf, SPI_FLASH_PAGE_SIZE * 2);
     printf("buf[2049] = %d...buf[2050] = %d.2051 = %d\n", buf[2049], buf[2050], buf[2051]);
#endif

    return 0;
}
#endif