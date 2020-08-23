/*******************************************************************
 * 
 * Resources / dependencies:
 * 
 * BCM2835 library (http://www.airspayce.com/mikem/bcm2835/)
 * 
 * To create a build with only the SVM30 monitor type: 
 *      make
 *
 *  To create a build with the SVM30 and SDS011 monitor type:
 *      make BUILD=SDS011 (requires the sds011 sub-directory)
 *
 * 
 * Hardware connection
 * svm30 pin    Raspberry Pi
 *
 * 1 SCL        SCL pin 5 / GPIO 3
 * 2 GND        GND
 * 3 VCC        +5V
 * 4 SDA        SDA pin 3 / GPIO 2
 * 
 * No need for external resistors as pin 3 and 5 have onboard 1k8 
 * pull-up resistors on the Raspberry Pi.
 * 
 * *****************************************************************
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation, either version 3 of the 
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

# include "svm30lib.h"
# include <getopt.h>
# include <signal.h>
# include <stdint.h>
# include <stdarg.h>
# include <time.h>
# include <stdlib.h>

#define version "1.0 / October 2019"
#define MAXBUF 100

#ifdef SDS011

#include "sds011/sdsmon.h"
SDSmon SDSm;

typedef struct sds
{
    char    port[MAXBUF];   // connected port (like /dev/ttyUSB0)
    bool    include;        // true = include in output
    float   value_pm25;     // measured value sds
    float   value_pm10;     // measured value sds
} sds;

#endif //SDS011

typedef struct svm_par
{
    /* option SVM30 parameters */
    uint16_t baselineCo2;       // baseline CO2 to set
    uint16_t baselineTVOC;      // baseline CO2 to set
    bool    setBaseline;       // set baseline
    bool    dev_info_only;     // only display device info.
    bool    measure;           // perform measurement test
    
    /* option program variables */
    uint16_t loop_count;        // number of measurement
    uint16_t loop_delay;        // loop delay in between measurements
    bool timestamp;             // add timestamp in output
    bool verbose;               // verbose level
    bool raw;                   // display H2 and Ethanol
    bool humComp;               // perform humidity compensation
    bool AirQual;               // display CO2 and TVOC
    bool HumTemp;               // display humidity and temperature
    bool DispBaseline;          // display baseline info
    bool DewPoint;              // display dewpoint
    bool AbsHum;                // display absolute humidity
    bool HeatInd;               // display heatindex
    bool tempCel;               // display temperature in Celcius
    
    /* to store the SVM30 values */
    struct svm_values v;

#ifdef SDS011                    // SDS monitor option
    /* include SDS info */
    struct sds sds;
#endif
} svm_par;

/* used as part of p_printf() */
bool NoColor=false;

/* global constructor */ 
SVM30 MySensor;

char progname[20];

/*********************************************************************
 * @brief Display in color
 * @param format : Message to display and optional arguments
 *                 same as printf
 * @param level :  1 = RED, 2 = GREEN, 3 = YELLOW 4 = BLUE 5 = WHITE
 * 
 * if NoColor was set, output is always WHITE.
 *********************************************************************/
void p_printf(int level, char *format, ...) {
    
    char    *col;
    int     coll=level;
    va_list arg;
    
    //allocate memory
    col = (char *) malloc(strlen(format) + 20);
    
    if (NoColor) coll = WHITE;
                
    switch(coll)  {
        case RED:
            sprintf(col,REDSTR, format);
            break;
        case GREEN:
            sprintf(col,GRNSTR, format);
            break;      
        case YELLOW:
            sprintf(col,YLWSTR, format);
            break;      
        case BLUE:
            sprintf(col,BLUSTR, format);
            break;
        default:
            sprintf(col,"%s",format);
    }

    va_start (arg, format);
    vfprintf (stdout, col, arg);
    va_end (arg);

    fflush(stdout);

    // release memory
    free(col);
}
 
/*********************************************************************
*  @brief close hardware and program correctly
**********************************************************************/
void closeout()
{
   /* reset pins in Raspberry Pi */
   MySensor.close();

#ifdef SDS011       // SDS011 monitor
    SDSm.close_sds();
#endif

   exit(EXIT_SUCCESS);
}

/*********************************************************************
* @brief catch signals to close out correctly 
* @param  sig_num : signal that was raised
* 
**********************************************************************/
void signal_handler(int sig_num)
{
    switch(sig_num)
    {
        case SIGINT:
        case SIGKILL:
        case SIGABRT:
        case SIGTERM:
        default:
            printf("\nStopping SVM30 monitor\n");
            closeout();
            break;
    }
}

/*****************************************
 * @brief setup signals 
 *****************************************/
void set_signals()
{
    struct sigaction act;
    
    memset(&act, 0x0,sizeof(act));
    act.sa_handler = &signal_handler;
    sigemptyset(&act.sa_mask);
    
    sigaction(SIGTERM,&act, NULL);
    sigaction(SIGINT,&act, NULL);
    sigaction(SIGABRT,&act, NULL);
    sigaction(SIGSEGV,&act, NULL);
    sigaction(SIGKILL,&act, NULL);
}

/*********************************************
 * @brief generate timestamp
 * 
 * @param buf : returned the timestamp
 *********************************************/  
void get_time_stamp(char * buf)
{
    time_t ltime;
    struct tm *tm ;
    
    ltime = time(NULL);
    tm = localtime(&ltime);
    
    static const char wday_name[][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    
    static const char mon_name[][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    sprintf(buf, "%.3s %.3s%3d %.2d:%.2d:%.2d %d",
    wday_name[tm->tm_wday],  mon_name[tm->tm_mon],
    tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
    1900 + tm->tm_year);
}

/****************************************************
 * @brief  initialise the variables to default values
 * @param svm : pointer to SVM30 parameters
 ***************************************************/
void init_variables(struct svm_par *svm)
{
    /* option SVM30 parameters */
    svm->baselineCo2 = 0;           // used for set baseline
    svm->baselineTVOC = 0;
    svm->setBaseline = false;
    svm->dev_info_only = false;     // only serial numbers
    svm->measure = false;           // no measure/self test
    
    /* option program variables */
    svm->loop_count = 10;           // number of measurement
    svm->loop_delay = 5;            // loop delay in between measurements
    svm->timestamp = false;        // NOT include timestamp in output
    svm->verbose = false;          // No verbose / debug
    svm->raw = false;              // No display H2 and Ethanol
    svm->humComp = false;          // No perform humidity compensation
    svm->AirQual = true;           // display CO2 and TVOC
    svm->HumTemp = true;           // display humidity and temperature
    svm->DispBaseline = false;     // No display baseline info
    svm->DewPoint = false;         // No display Dew point
    svm->AbsHum = false ;          // No display absolute humidity
    svm->HeatInd = false;          // No display Heat index
    svm->tempCel = true;           // display temperature in celsius
    
#ifdef SDS011
    /* SDS values */
    svm->sds.include = false;
    svm->sds.value_pm25 = 0;
    svm->sds.value_pm10 = 0;
#endif
}

/**********************************************************
 * @brief initialise the Raspberry PI and SVM30 / SDS011
 * @param svm : pointer to SVM30 parameters
 *********************************************************/
void init_hw(struct svm_par *svm)
{
    /* progress & debug messages tell driver */
    MySensor.EnableDebugging(svm->verbose);

    /* sync temperature setting between lib and program */
    MySensor.SetTempCelsius(svm->tempCel); 
    
    if (! MySensor.begin()) {
        p_printf(RED,(char *)"Error during setting I2C\n");
        exit(EXIT_FAILURE);
    }

#ifdef SDS011  // SDS011 monitor
    if (svm->sds.include) {
    
        if (svm->verbose) p_printf (YELLOW, (char *) "initialize SDS011\n");
        
        if (SDSm.open_sds(svm->sds.port, svm->verbose) != 0) {
            p_printf (RED, (char *) "Could NOT connect to SDS011\n");
            closeout();
        }
        
        if (svm->verbose) p_printf (YELLOW, (char *) "connected to SDS011\n");
    }
#endif // SDS011
}

#ifdef SDS011
/**
 * @brief read and display SDS011 information
 * @param svm : stored values
 * 
 * @return
 * false : no display done
 * true  ; display done
 */
bool sds_output(struct svm_par *svm)
{
    /* if no SDS device specified */
    if ( ! svm->sds.include) return(false);
    
    // read values from SDS
    if (SDSm.read_sds(&svm->sds.value_pm25, &svm->sds.value_pm10) != 0)
    {
        p_printf(RED, (char*) "error during reading sds\n");
        return(false);
    }

    p_printf(GREEN, (char *)"SDS011\t\tPM2.5:\t%-4.4f\t\tPM10:\t\t%-4.4f\n",
    svm->sds.value_pm25, svm->sds.value_pm10 );

    return(true);
}
#endif // SDS011

/*****************************************************************
 * @brief : output the results
 * 
 * @param svm : pointer to SVM30 parameters
 ****************************************************************/
void do_output(struct svm_par *svm)
{
    char buf[30];
    bool output = false;
    uint32_t baseline;

    if (svm->timestamp)  {
        get_time_stamp(buf);
        p_printf(YELLOW, (char *) "%s\n",buf);
    }
               
    // format output of the data
    if (svm->AirQual) {
        p_printf(GREEN,(char *) "CO2 equivalent\t\t%-5d\t\tTVOC\t\t%-5d\n",svm->v.CO2eq, svm->v.TVOC);
        output = true;
    }
    
    if (svm->raw) {
        p_printf(GREEN,(char *) "H2 signal\t\t0x%-4X\t\tEthanol signal\t0x%-4X\n","H2 signal",svm->v.H2_signal, svm->v.Ethanol_signal);
        output = true;
    }
    
    // Celsius or Fahrenheit
    if (svm->tempCel) buf[0] = 'C';
    else  buf[0] = 'F';
        
    if(svm->HumTemp) {
        p_printf(GREEN,(char *) "Humidity\t\t%-6.2f%%\t\tTemperature\t%-2.2f %c\n",
        (float) svm->v.humidity / 1000, (float) svm->v.temperature / 1000, buf[0]); 
        output = true;
    }

    if (svm->DewPoint) {
        p_printf(GREEN,(char *) "DewPoint\t\t%-2.2f %c\n",
        svm->v.dew_point, buf[0]);
        output = true;
    }
    
    if (svm->HeatInd) {
        p_printf(GREEN,(char *) "Heat index\t\t%-2.2f %c\n",
        svm->v.heat_index, buf[0]);
        output = true;
    }       
    
    if (svm->AbsHum) {
        p_printf(GREEN,(char *) "Absolute Humidity\t%-2.2f %%\n",
        (float) svm->v.absolute_hum);
        output = true;
    }   

    if(svm->DispBaseline) {
        if (MySensor.GetBaseLines(&baseline)) {
            // will return 0x0 in the first 15 seconds after reset/start
            p_printf(GREEN,(char *) "TVOC baseline\t\t0x%-04X\t\tCO2 Baseline\t0x%-04X\n"
            ,baseline >> 16 , baseline & 0xffff);
            output = true;
        }
    } 
    
#ifdef SDS011
    if (sds_output(svm)) output = true;
#endif // SDS011

    if (output) p_printf(WHITE, (char *) "\n");
    else p_printf(RED, (char *) "Nothing selected to display \n");
}

/*****************************************************************
 * @brief : Display the device information
 * @param svm : pointer to SVM30 parameters
 ****************************************************************/
uint8_t disp_dev(struct svm_par *svm)
{
    uint16_t    id[3];
    char        buf[2];
    
    p_printf(YELLOW, (char *) "Driver info : %s\n", MySensor.GetDriverVersion());
    
    /* get the ID number (check that communication works) */
    if(! MySensor.GetId(SGP30,id)) {
       p_printf (RED, (char *) "Error during getting SGP30 ID number\n");
       return(ERR_PROTOCOL);
    }
    
    p_printf(YELLOW, (char *) "SGP30 ID : 0x%04X %04X %04X\n", id[0], id[1],id[2]);
 
     /* get the ID number (check that communication works) */
    if(! MySensor.GetId(SHTC1,id)) {
       p_printf (RED, (char *) "Error during getting SHTC1 ID number\n");
       return(ERR_PROTOCOL);
    }
    
    p_printf(YELLOW, (char *) "SHTC1 ID : 0x%04X\n", id[0] & 0x3f);
       
     /* get the ID number (check that communication works) */
    if(! MySensor.GetFeatureSet(buf)) {
       p_printf (RED, (char *) "Error during getting feature set\n");
       return(ERR_PROTOCOL);
    }
    
    p_printf(YELLOW, (char *) "SGP30 product ID : 0x%02X, feature set 0x%2X\n", buf[0] & 0x3f, buf[1]);
    
    return(ERR_OK);
}

/*****************************************************************
 * @brief : Set baseline(s)
 * @param svm : pointer to SVM30 parameters
 * 
 * source : datasheet
 * After a power-up or soft reset, the baseline of the baseline compensation
 * algorithm can be restored by sending first an “Init_air_quality” command followed
 * by a “Set_baseline” command with the two baseline values as parameters
 * in the order as (TVOC, CO 2 eq)
 * 
 * Although baselines can be set from the start, it is better to wait until
 * the SVM30 is ready with first calibration :
 * 1. setting TVOC while CO2 baseline is still zero, will result in CO2 baseline
 * being set.
 * 2. if setting CO2 followed by TVOC during calibration will stop any adjustments
 * during reading (the values are stalled and not further optimzed)
 ****************************************************************/

bool set_baseline(struct svm_par *svm)
{
   uint16_t baseline =0;
   
   if (! MySensor.GetBaseLine_TVOC(&baseline)) {
        // will return 0x0 in the first 15 seconds after reset/start
        p_printf(RED,(char *) "Error during reading baseline\n");
        return(false);
    }
   
   if (baseline == 0) return(true);
   
    if(svm->baselineCo2 > 0) {
        if (! MySensor.SetBaseLine_CO2(svm->baselineCo2)) {
            p_printf (RED, (char *) "Error during getting Setbaseline CO2\n");
            return(false);
        }
    }

    if(svm->baselineTVOC > 0) {
        if (! MySensor.SetBaseLine_TVOC(svm->baselineTVOC)) {
            p_printf (RED, (char *) "Error during getting Setbaseline TVOC\n");
            return(false);
        }
    }
    svm->setBaseline = false;
    
    return(true);
}

/*****************************************************************
 * @brief : set humidity compensation
 * @param svm : pointer to SVM30 parameters
 ****************************************************************/
bool do_humidityComp(struct svm_par *svm)
{
    // if NOT requested, return
    if (! svm->humComp) return(true); 
    
    if (! MySensor.SetHumidity(svm->v.absolute_hum)) {
        p_printf (RED, (char *) "Error during setting humidity compensation\n");
        return(false);
    }
    
    return(true);
}
/*****************************************************************
 * @brief Here is the main of the program 
 * @param svm : pointer to SVM30 parameters
 ****************************************************************/
void main_loop(struct svm_par *svm)
{
    int     loop_set;
    uint8_t  wait;
   
    if (disp_dev(svm) != ERR_OK) return;
    
    /* if only device info was requested */
    if (svm->dev_info_only) return;
    
    p_printf(GREEN,(char *) "Starting SVM30 measurement:\n");

    /* check for measurement test request */
    if(svm->measure) {
        
        if (MySensor.MeasureTest()) 
            p_printf(BLUE,(char *)"MeasureTest completed\n");
        else
            p_printf(RED,(char *)"MeasureTest failed\\n");
    }
       
    /*  check for endless loop */
    if (svm->loop_count > 0 ) loop_set = svm->loop_count;
    else loop_set = 1;
    
    /* loop requested */
    while (loop_set > 0)  {
        
    /* check for setting new baseline 
     * must done no earlier than 15 seconds after init
     * The end of calibration is detected as the read
     * baseline gives a value other than 0
     * This is handled in set_baseline() and will reset
     * the setBaseline flag */
        if(svm->setBaseline) {
            if (!set_baseline(svm)) return;
        }
        
        if (MySensor.GetValues(&svm->v,svm->raw)) {
            do_output(svm);
        }
        else  {
            p_printf(RED,(char *)"failed get values from SVM30\n");         
            return;
        }
        
        // check and do humidity compensation
        if (! do_humidityComp(svm)) return;

        /* delay for seconds */
        wait = svm->loop_delay;
        while (wait--) {
            sleep(1);       // sleep 1 second
            if( !MySensor.TriggerSGP30()) {
                p_printf(RED,(char *)"Error during loop delay\n");
                return;
            }
        } 
        
        /* check for endless loop */
        if (svm->loop_count > 0) loop_set--;
    }
    
    printf("Reached the loopcount of %d.\nclosing down\n", svm->loop_count);
}       

/*********************************************************************
* @brief usage information  
* @param svm : pointer to SVM30 parameters
**********************************************************************/
void usage(struct svm_par *svm)
{
    // reset in case some changed before incorrect option was encountered
    init_variables(svm);
    
    printf("%s [options]  (version %s) \n\n"
    
    "SVM30 settings: \n"
    "-c 0x# set baseline CO2  to ####\n"
    "-t 0x# set baseline TVOC to ####\n"
    "-h     continued humidity compensation          (default %s)\n"
    "-m     perform a measurement test               (default %s)\n"
    
    "\nprogram control settings\n"
    "-d     display ID-numbers and feature set only\n"
    "-l #   number of measurements (0 = endless)     (default %d)\n"
    "-w #   wait-time (seconds) between measurements (default %d)\n"
    "-v     include verbose / debug information      (default %s)\n"
    
    "\noutput formatting\n"
    "-D     do not display output in color           (default %s)\n"
    "-T     add / remove timestamp                   (default %s)\n"
    "-H     add / remove humidity & temperature      (default %s)\n"
    "-A     add / remove CO2 / TVOC info             (default %s)\n"
    "-B     add / remove baseline info               (default %s)\n"
    "-R     add / remove H2 and Ethanol signals      (default %s)\n"
    "-E     add / remove Dew point calculation       (default %s)\n"
    "-J     add / remove Absolute Humidity calc      (default %s)\n"
    "-G     add / remove HeatIndex calc              (default %s)\n"
    "-F     Display temperature (Fahrenheit/Celsius) (default %s)\n"

#ifdef SDS011
    "\nSDS011:\n"
    "-S port    Enable SDS011 input from port        (No default)\n"
#endif    
   , progname, version, 
   svm->humComp?"enabled":"disabled",
   svm->measure?"enabled":"disabled",
   svm->loop_count, svm->loop_delay, 
   svm->verbose?"added":"removed",
   NoColor?"No color":"color",
   svm->timestamp?"added":"removed",  
   svm->HumTemp?"added":"removed", 
   svm->AirQual?"added":"removed",
   svm->DispBaseline?"added":"removed",
   svm->raw?"added":"removed",
   svm->DewPoint?"added":"removed",
   svm->AbsHum?"added":"removed",
   svm->HeatInd?"added":"removed",
   svm->tempCel?"Celcius":"Fahrenheit");
}

/*********************************************************************
 * Parse parameter input 
 * @param svm : pointer to SVM30 parameters
 * @param opt: option character
 * @param option : option argunment
 *********************************************************************/ 

void parse_cmdline(int opt, char *option, struct svm_par *svm)
{
    switch (opt) {
                
    case 'm':   // set self test
        svm->measure = true;
        break;

    case 'c':   // SVM30 CO2 baseline
        svm->baselineCo2 = (uint32_t) strtod(option, NULL);
        svm->setBaseline = true;
        // must be valid
        if (svm->baselineCo2 == 0 )
        {
            p_printf (RED, (char *) "Incorrect baseline Co2. Must be positive\n");
            exit(EXIT_FAILURE);
        }
        break;

    case 't':   // SVM30 tvoc baseline
        svm->baselineTVOC = (uint32_t) strtod(option, NULL);
        svm->setBaseline = true;
        // must be valid
        if (svm->baselineTVOC == 0 )
        {
            p_printf (RED, (char *) "Incorrect baseline TVOC. Must be positive\n");
            exit(EXIT_FAILURE);
        }
        break;
 
    case 'h':   // SVM30 continued humidity compensation 
        svm->humComp = true;
        break;
 
    case 'd':   // display device info only
        svm->dev_info_only = true;
        break;                           
   
    case 'A':   // toggle add / remove CO2 / TVOC info 
        svm->AirQual = ! svm->AirQual;
        break;

    case 'E':   // toggle add / remove dewpoint
        svm->DewPoint = ! svm->DewPoint;
        break;

    case 'J':   // toggle add / remove Absolute Humidity
        svm->AbsHum = ! svm->AbsHum;
        break;
        
    case 'G':   // toggle display Heat index
        svm->HeatInd = ! svm->HeatInd;
        break;

    case 'F':   // toggle display temperature Fahrenheit /celsius
        svm->tempCel = ! svm->tempCel;
        break;
                               
    case 'H':   // toggle display humidity and temperature
        svm->HumTemp = ! svm->HumTemp;
        break;

    case 'B':   // toggle baseline info  
        svm->DispBaseline = ! svm->DispBaseline;
        break;
        
     case 'R':   // toggle reading and display H2 and Ethanol signals
        svm->raw = ! svm->raw;
        break;       
                
    case 'D':   // set for no color output
        NoColor = true;
        break; 
              
    case 'l':   // loop count
        svm->loop_count = (uint16_t) strtod(option, NULL);
        if (svm->loop_delay == 0 ) {
            p_printf (GREEN, (char *) "Endless loop selected\n");
        } 
        break;
          
    case 'w':   // loop delay in between measurements
        svm->loop_delay = (uint16_t) strtod(option, NULL);
        if (svm->loop_delay == 0 ) {
            p_printf (RED, (char *) "Incorrect loop delay. Must be larger than zero\n");
            exit(EXIT_FAILURE);
        }
        break;
    
    case 'T':  // toggle timestamp to output
        svm->timestamp = ! svm->timestamp;
        break;
                
    case 'v':   // set verbose / debug level
        svm->verbose = ! svm->verbose;

        // must be between 0 and 2

        break;
      
    case 'S':   // include SDS011 read
#ifdef SDS011        
        strncpy(svm->sds.port, option, MAXBUF);
        svm->sds.include = true;
#else
        p_printf(RED, (char *) "SDS011 is not supported in this build\n");
#endif
        break;    
    
    default: /* '?' */
        usage(svm);
        exit(EXIT_FAILURE);
    }
}

/***********************
 *  program starts here
 **********************/
int main(int argc, char *argv[])
{
    int opt;
    struct svm_par svm; // parameters

    if (geteuid() != 0)  {
        p_printf(RED,(char *) "You must be super user\n");
        exit(EXIT_FAILURE);
    }
    
    /* set signals */
    set_signals(); 
 
    /* save name for (potential) usage display */
    strncpy(progname,argv[0],20);
    
    /* set the initial values */
    init_variables(&svm);

    /* parse commandline */
    while ((opt = getopt(argc, argv, "c:t:hmdl:w:vDEFJTAGHBRP:S:")) != -1) {
        parse_cmdline(opt, optarg, &svm);
    }

    /* initialise hardware */
    init_hw(&svm);

    /* main loop to read SVM30 results */
    main_loop(&svm);

    closeout();
}
