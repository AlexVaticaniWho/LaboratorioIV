#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "rs232.h"

int lostedInfo = 0, cnt = 0;

// buf è un vettore di 4096 byte (char) organizzati in char
// misurazioni è un array di 4 bytes (char)
unsigned char buf[4096], misurazioni[4];

// Funzioni per la decodifica e l'applicazione delle correzioni all'umidità

double decodeTemperature(unsigned int rbuf);
double decodeHumidity(unsigned int rbuf);
double corrHumidity(double hum_val, unsigned int rbuf, double temperature_ref);
void acquisizione(int n, int *nloc, struct tm *gmp_run, FILE *file, FILE *data,  int rebuildPackages);

int main(int argc, char *argv[])
{

  // Inizializzazione delle variabili tra cui le variabili di tempo. Le struct sono antenati degli oggetti: le prime due variabili sono istanze di struct.

  struct tm *gmp, *gmp_run;
  // struct timeval *utime, *utime_run; //tempo iniziale in microsecondi e tempo della run in microsecondi
  time_t t0, t, t0_usec, t_usec;
  int ty, tmon, tday, thour, tmin, tsec, time_acq_h_MAX;
  double time_acq_sec;
  int nloc, cport_nr = 17, bdrate = 115200, sleep_time = 400;

  FILE *file;
  FILE *datiraw;
  FILE *currN; // file che salva il nome del file corrente affinch� possa essere usato da altri programmi

  char NameF[100];

  char mode[] = {'8', 'N', '1', 0}; // 8 bit, no parity, 1 bit di stop, no controllo del flusso

  t0 = time(NULL); // timer start
  gmp = gmtime(&t0);

  /*gettimeofday(&utime, NULL);
  t0_usec = utime.tv_usec;*/

  if (gmp == NULL)
  {
    printf("error on gmp");
    return 1;
  }

  ty = gmp->tm_year + 1900;
  tmon = gmp->tm_mon + 1;
  tday = gmp->tm_mday;
  thour = gmp->tm_hour + 1;
  tmin = gmp->tm_min;
  tsec = gmp->tm_sec;

  if (argv[1] == NULL)
  {
    printf("format: read_rx Numero di ore di acquisizione \n");
    return -1;
  }

  else
  {
    time_acq_h_MAX = atoi(argv[1]); // numero di ore massimo (int)
    sprintf(NameF, "sht75_nblab03_Hum_Temp_RUN_%04d%02d%02d%02d%02d%02d_%d_h.txt", ty, tmon, tday, thour, tmin, tsec, time_acq_h_MAX);
    printf("file_open %s --> durata in ore %d\n", NameF, time_acq_h_MAX);
    file = fopen(NameF, "w+");
  }

  /***scrivo il nome del file nel file nome corrente affinch� possa essere usato da programmi esterni***/

  currN = fopen("currN.txt", "w");
  fprintf(currN, NameF);
  fclose(currN);

  /***********se ci sono problemi****************/

  if (RS232_OpenComport(cport_nr, bdrate, mode))
  { // sottinteso if (RS232_OpenComport() =1) perchè open comport restituisce 1 in caso di errore

    printf("Can not open comport\n");
    return (0);
  }

  datiraw = fopen("datiraw.txt", "w");
  nloc = -1;

  while (1)
  { // itero per un numero infinito di volte (condizione sempre verificata)

    int n = RS232_PollComport(cport_nr, buf, 4095); // numero di bytes ricevuti attraverso la porta seriale, messi in buf il cui massimo valore rappresentabile è 4095

    /*********stimo la durata dell'acquisizione dati**************/

    t = time(NULL);
    gmp_run = gmtime(&t);

    /*gettimeofday(&utime_run, NULL);
    t_usec = gmp_run->tv_usec;*/

    if (gmp_run == NULL)
    {
      printf("error on gmp_run");
    }
    else
    {
      printf("Number of packets received: %d ", n);

      time_acq_sec = difftime(t, t0); // Tempo trascorso dall'inzio della presa dati

      printf("Time elapsed: %f (sec) \n", time_acq_sec);

      if (time_acq_sec > time_acq_h_MAX * 3600)
      {

        printf("Time_duration RUN in minutes > %d \n", time_acq_h_MAX * 60);

        break;
      }

      else if (cnt % 100 == 0)
      { // cambia alla fine del while(1)
        printf("Time current in hour %f \n", time_acq_sec / 3600.);
      }

      /***********acquisizione dati*************/
      if (n > 0)
      {
        nloc++;

        if (lostedInfo > 0)
        { // Caso in cui devo ricostruire il pacchetto
          printf("Starting rebuilding packages! \n");

          acquisizione(6 - lostedInfo, &nloc, gmp_run, file, datiraw, 1);
        }
        acquisizione(n, &nloc, gmp_run, file, datiraw, 0);
      }

      printf("Bytes received: %i\n", n);

      cnt++;
    }

    #ifdef _WIN32
        Sleep(sleep_time); // sospende temporaneamente il processo per sleep_time(ms)
    #else
        usleep(sleep_time * 1000);
    #endif
      }
      fclose(file);
      fclose(datiraw);

      return (0);
}

double decodeTemperature(unsigned int rbuf)
{
  double d1, d2, rd_val;
  d1 = -39.6;
  d2 = 0.01;
  rd_val = (double)rbuf + 0.;
  return d1 + d2 * rd_val;
}

double decodeHumidity(unsigned int rbuf)
{
  double c1, c2, c3, rd_val, hum_val;
  c1 = -2.0468;
  c2 = 0.0367;
  c3 = -1.5955e-6;

  rd_val = (double)rbuf;
  hum_val = c1 + c2 * rd_val + c3 * (rd_val) * (rd_val);

  return hum_val;
}

double corrHumidity(double hum_val, unsigned int rbuf, double temperature_ref)
{
  double t1, t2, rd_val, hum_val_corrected;
  t1 = 0.01;
  t2 = 0.00008;
  rd_val = (double)rbuf;
  hum_val_corrected = (temperature_ref - 25) * (t1 + t2 * rd_val) + hum_val;
  return hum_val_corrected;
}

void acquisizione(int n, int *nloc, struct tm *gmp_run, FILE *file, FILE *data, int rebuildPackages)
{
  printf("Acquiring data... \n"); 
  int InitFlag = 0, StartFlag, nhit, hit, trg = 0, nresto, i, k = 0;
  double val_temp, val_hum, val_hum_corr;
  unsigned int val_temp_int, val_hum_int;

  // buf[n] = 0;   // l'(n+1)esimo byte lo poniamo nullo quando stiamo acquisendo i primi n
  nhit = n / 6; // numero di pacchetti di dati (due bytes di controllo, due di RH e due di T)
  hit = 0;
  nresto = n % 6;
  printf("nhit %d \n", nhit);

      if (rebuildPackages || lostedInfo == 0)
      { // Caso in cui sto recuperando informazioni dalla precedente acquisizione
        i = 0;
      }
      else
      {
        // Se lostedInfo è diverso da 0 allora nel for successivo inizio a ciclare da 6 - lostedinfo
        i = 6 - lostedInfo;
      }

  StartFlag = 0;

  int index;
  for (index = i; index < n; index++)
  {
    // index va da 0 a n-1, con n la lunghezza dei byte presi. Per ogni index avremo Misurazione_nloc = buf_i
    // quando index = 0 e index = 1, gira a vuoto perché initFlag=0
    if (buf[index] == 0xAA && buf[index - 1] == 0xAA)
    {
      *nloc = 0;
      printf( "\n buf[0] = %x and buf[1] = %x \n", buf[index], buf[index-1]);

      if (InitFlag == 0)
      {
        InitFlag = 1;
      }
    }

    if (StartFlag == 1) // Questa condizione è verificata nel caso sia già terminato il processo di stampa di almeno un pacchetto intero
    {
      *nloc = *nloc - 5 * k;
    }

    if (InitFlag == 1)
    {
      // a questo punto abbiamo nloc=0 e index=2
      // parte dal terzo byte, dopo i primi due di controllo. Ciò è determinato dal primo if.
      misurazioni[*nloc] = buf[index]; // a partire dal terzo byte di buf riempie il primo di misurazioni corrispondente a nloc=0

      if (*nloc == 1)
      {
        /*OR bit a bit (restituisce un int) tra il primo byte traslato e il secondo;
        in questo modo mettiamo in sequenza i due byte di umidità relativa e li leggiamo
        come un unico valore binario*/
        val_hum_int = (misurazioni[0] << 8) | (misurazioni[1]);
        val_hum = decodeHumidity(val_hum_int);

        printf("\n val_hum = %.2f and nloc = %d \n", val_hum, *nloc);

        trg++;
        hit++;
      }

      else if (*nloc == 3)
      {

        printf("Pacchetto %d\n%3x%3x\n%3x%3x\n%3x%3x\n\n", trg, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
        // Stesso processo logico eseguito per l'umidità
        val_temp_int = (misurazioni[2] << 8) | (misurazioni[3]);
        val_temp = decodeTemperature(val_temp_int);
        val_hum_corr = corrHumidity(val_hum, val_hum_int, val_temp);

        if (cnt % 100 == 0)
        {
          printf(" read_Hum MSB %x - LSB %x --> Hum16bitRaw  %x - HumReco %.2f (dec)\n", misurazioni[0], misurazioni[1], val_hum_int, val_hum_corr);
          printf(" read Temp MSB %x - LSB %x --> Temp16bitRaw %x - TempReco %.2f (dec)\n", misurazioni[2], misurazioni[3], val_temp_int, val_temp);

        }
        
        fprintf(file, "%d\t%d\t%d\t%d\t%.1f\t", trg, gmp_run->tm_year + 1900, gmp_run->tm_mon + 1, gmp_run->tm_mday, 3600 * gmp_run->tm_hour + 60 * gmp_run->tm_min + gmp_run->tm_sec + (double)hit * 4 / (double)nhit);
        fprintf(file, "\t%d\t%.2f\t", val_hum_int, val_hum_corr);
        fprintf(file, "%d\t%.2f\n", val_temp_int, val_temp);
        fprintf(data, "Pacchetto %d\n%3x%3x\n%3x%3x\n%3x%3x\n\n", trg, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
        fflush(stdout);
      }

      else if (*nloc > 5)
      {
        printf(" more than expected \n");

        StartFlag = 1;
        k++;
      }
    }

    printf("\n nloc prima di nloc++ = %d",  *nloc);

    (*nloc)++; // faccio crescere ad ogni iterazione nloc che tiene conto delle coppie di bytes acquisite

    //&& se entrambi gli operatori sono diversi da zero (veri) � una condizione vera
    // 0x sta per "la seguente è una cifra esadecimale" AA è il byte di controllo in esadecimale
    // cerchiamo due bytes di controllo AA consecutivi. è questo il comando che fa scorrere la i e comporta l'inizio dal terzo byte del primo if.
  }
  lostedInfo = nresto;
}