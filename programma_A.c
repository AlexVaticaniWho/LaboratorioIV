/**************************************************

compile with the command: gcc read_rx.c rs232.c -Wall -Wextra -o2 -o read_rx

**************************************************/

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

// Funzioni per la decodifica e l'applicazione delle correzioni all'umidità

double decodeTemperature(unsigned int rbuf);
double decodeHumidity(unsigned int rbuf);
double corrHumidity(double hum_val, unsigned int rbuf, double temperature_ref);
float time_diff(struct timeval *t0_usecond, struct timeval *t_usecond)


int main(int argc, char *argv[]) {

  // Inizializzazione delle variabili tra cui le variabili di tempo. Le struct sono antenati degli oggetti: le prime due variabili sono istanze di struct.

  struct tm *gmp, *gmp_run;
  struct tm gm;
  time_t t, t0;
  int ty, tmon, tday, thour, tmin, tsec, time_acq_h_MAX;
  float time_acq_sec;
  int i, n, nloc, InitFlag, StartFlag, nhit, hit, trg, cport_nr = 17, bdrate = 115200, sleep_time = 400;
  int cnt = 0;

  struct timeval t0_usecond, t_usecond;

  FILE *file;
  FILE *currN; // file che salva il nome del file corrente affinch� possa essere usato da altri programmi
  double val_temp, val_hum, val_hum_corr;
  unsigned char buf[4096], sht75_nblab03_frame[4]; // buf è un vettore di 4096 byte (char) organizzati in char
                                                   // sht75_nblab03_frame è un array di 4 bytes (char)
  unsigned int val_temp_int, val_hum_int;
  char NameF[100];

  char mode[] = {'8', 'N', '1', 0}; // 8 bit, no parity, 1 bit di stop, no controllo del flusso

  t0 = time(NULL); // timer start
  gmp = gmtime(&t0);

  if (gmp == NULL) {
    printf("error on gmp");
    return;
  }

  gettimeofday(&t0_usecond, NULL);

  ty = gmp->tm_year + 1900;
  tmon = gmp->tm_mon + 1;
  tday = gmp->tm_mday;
  thour = gmp->tm_hour + 1;
  tmin = gmp->tm_min;
  tsec = gmp->tm_sec;

  if (argv[1] == NULL) {

    printf("format: read_rx Numero di ore di acquisizione \n");
    return -1;

  }

  else {

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

  if (RS232_OpenComport(cport_nr, bdrate, mode)) { // sottinteso if (RS232_OpenComport() =1) perchè open comport restituisce 1 in caso di errore
  
    printf("Can not open comport\n");
    return (0);

    // break;
  }

  InitFlag = 0;
  nloc = 0;
  trg = 0;

  while (1) { // itero per un numero infinito di volte (condizione sempre verificata)

    n = RS232_PollComport(cport_nr, buf, 4095); // numero di bytes ricevuti attraverso la porta seriale, messi in buf il cui massimo valore rappresentabile è 4095

    /*********stimo la durata dell'acquisizione dati**************/

    t = time(NULL);
    gmp_run = gmtime(&t);
    gettimeofday(&t_usecond, NULL);


    if (gmp_run == NULL)
      printf("error on gmp_run");
    else {
      printf("%d %d ", cnt, n);

      time_acq_sec = (t - t0); // Tempo trascorso dall'inzio della presa dati

      printf("Time elapsed: %f (sec) \n",time_acq_sec);

      if (time_acq_sec > time_acq_h_MAX * 3600) {

        printf("Time duration RUN in minutes > %d \n", time_acq_h_MAX * 60);
        break;
      }

      else if (cnt % 100 == 0) {// cambia alla fine del while(1)
          printf("Time current in hour %f \n", time_acq_sec / 3600.);
      }

      /***********acquisizione dati*************/

      if (n > 0) {

        buf[n] = 0;   // l'(n+1)esimo byte lo poniamo nullo quando stiamo acquisendo i primi n
        nhit = n / 6; // numero di pacchetti di dati (due bytes di controllo, due di RH e due di T)
        hit = 0;
        printf("nhit %d \n", nhit);

        for (i = 0; i < n; i++) {
          

          if (InitFlag == 1) {
            
            /* parte dal terzo byte, dopo il primo di controllo. Ciò è determinato dall'ultimo if che controlla AA-AA.
            a partire dal terzo byte riempie il primo bit corrispondente a nloc=0 */
            sht75_nblab03_frame[nloc] = buf[i]; 

            if (nloc == 1) {

              /*OR bit a bit (restituisce un int) tra il primo byte traslato e il secondo;
              in questo modo mettiamo in sequenza i due byte di umidità relativa e li leggiamo
              come un unico valore binario*/
              val_hum_int = (sht75_nblab03_frame[0] << 8) | (sht75_nblab03_frame[1]);
              val_hum = decodeHumidity(val_hum_int);

              trg++;
              hit++;
            }

            else if (nloc == 3) {

              // Stesso processo logico eseguito per l'umidità
              val_temp_int = (sht75_nblab03_frame[2] << 8) | (sht75_nblab03_frame[3]);
              val_temp = decodeTemperature(val_temp_int);
              val_hum_corr = corrHumidity(val_hum, val_hum_int, val_temp);

              if (cnt % 100 == 0) {
                printf(" read_Hum MSB %x - LSB %x --> Hum16bitRaw  %x - HumReco %.2f (dec)\n", sht75_nblab03_frame[0], sht75_nblab03_frame[1], val_hum_int, val_hum_corr);
                printf(" read Temp MSB %x - LSB %x --> Temp16bitRaw %x - TempReco %.2f (dec)\n", sht75_nblab03_frame[2], sht75_nblab03_frame[3], val_temp_int, val_temp);
              }

              fprintf(file, "%d\t%d\t%d\t%d\t%.1f\t%.1f", trg, gmp->tm_year + 1900, gmp->tm_mon + 1, gmp->tm_mday, 3600 * gmp->tm_hour + 60 * gmp->tm_min + gmp->tm_sec + (double)hit * 4 / (double)nhit), time_diff(t0_usecond, t_usecond);
              fprintf(file, "\t%d\t%.2f\t", val_hum_int, val_hum_corr);
              fprintf(file, "%d\t%.2f\n", val_temp_int, val_temp);
            }

            else if (nloc > 5) {
              printf(" error on nloc\n");
            }
          }

          nloc++; // faccio crescere ad ogni iterazione nloc che tiene conto delle coppie di bytes acquisite

          /*&& se entrambi gli operatori sono diversi da zero (veri) è una condizione vera 
          0x sta per "la seguente è una cifra esadecimale" AA è il byte di controllo in esadecimale 
          cerchiamo due bytes di controllo AA consecutivi. è questo il comando che fa scorrere la i e comporta l'inizio dal terzo byte del primo if.*/
          if (i > 0 && buf[i] == 0xAA && buf[i - 1] == 0xAA) {          
            StartFlag = 1;
            nloc = 0;

            if (InitFlag == 0) {
              InitFlag = 1;
            }
          }
        }
        printf("cnt %d received %i bytes \n", cnt, n);
        cnt++;
      }
    }

    #ifdef _WIN32
      Sleep(sleep_time); // sospende temporaneamente il processo per il tempo prescelto in ms
    #else
      usleep(sleep_time * 100);
    #endif
  }

  fclose(file);

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

float time_diff(struct timeval *t0_usecond, struct timeval *t_usecond)
{
    return 1e-3*(t_usecond->tv_usec - t0_usecond->tv_usec);
}

