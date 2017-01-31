#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>
#include "typedef.h"
#include "IBonDriver2.h"
#include "recpt1core.h"
#include "version.h"
#include "pt1_dev.h"

/* globals */
boolean f_exit = FALSE;
stChannel g_stChannels[MAX_CH];

// linux環境下で標準的なチャンネル指定をSetChannel()に渡せる形に変換
BON_CHANNEL_SET *
searchrecoff(DWORD dwSpace, char *channel)
{
	static BON_CHANNEL_SET channel_set;
	DWORD dwBonSpace = dwSpace;
	DWORD dwBonChannel = ARIB_CH_ERROR;
	DWORD freq = 0;
	int type = CHTYPE_BonNUMBER;

	if ((*channel == 'B' || *channel == 'b') && isdigit(*(channel + 1)))
	{
		dwBonChannel = 0;
		while (isdigit(*++channel))
		{
			dwBonChannel *= 10;
			dwBonChannel += *channel - '0';
		}
	}
	else
	{
		// recbond.confのチャンネル定義と照合
		int i = 0;
		while (g_stChannels[i].channel[0] != '\0')
		{
			if (!strcmp(channel, g_stChannels[i].channel))
			{
				dwBonSpace = g_stChannels[i].bon_space;
				dwBonChannel = g_stChannels[i].bon_num;
				break;
			}
			i++;
		}
		if (dwBonChannel == ARIB_CH_ERROR)
		{
			if (isdigit(*channel))
			{
				freq = atoi(channel);
				if (freq > 90000 && freq < 770000)
					type = CHTYPE_GROUND;
				else if (freq > 1032000 && freq < 2602000)
					type = CHTYPE_SATELLITE;
				else
					return NULL;
			}
			else
				return NULL;
		}
	}

	channel_set.bon_space = dwBonSpace;
	channel_set.bon_num = dwBonChannel;
	channel_set.set_freq = freq;
	channel_set.type = type;

	return &channel_set;
}

int
open_tuner(thread_data *tdata, char *driver)
{
	// モジュールロード
	tdata->hModule = dlopen(driver, RTLD_LAZY);
	if(!tdata->hModule) {
		fprintf(stderr, "Cannot open tuner driver: %s\ndlopen error: %s\n", driver, dlerror());
		return -1;
	}

	// インスタンス作成
	tdata->pIBon = tdata->pIBon2 = NULL;
	char *err;
	IBonDriver *(*f)() = (IBonDriver *(*)())dlsym(tdata->hModule, "CreateBonDriver");
	if ((err = dlerror()) == NULL)
	{
		tdata->pIBon = f();
		if (tdata->pIBon)
			tdata->pIBon2 = dynamic_cast<IBonDriver2 *>(tdata->pIBon);
	}
	else
	{
		fprintf(stderr, "dlsym error: %s\n", err);
		dlclose(tdata->hModule);
		return -2;
	}
	if (!tdata->pIBon || !tdata->pIBon2)
	{
		fprintf(stderr, "CreateBonDriver error: tdata->pIBon[%p] tdata->pIBon2[%p]\n", tdata->pIBon, tdata->pIBon2);
		dlclose(tdata->hModule);
		return -3;
	}

	// ここから実質のチューナオープン & TS取得処理
	BOOL b = tdata->pIBon->OpenTuner();
	if (!b)
	{
		tdata->pIBon->Release();
		dlclose(tdata->hModule);
		return -4;
	}else
		tdata->tfd = TRUE;

	return 0;
}

int
close_tuner(thread_data *tdata)
{
	int rv = 0;

	if(tdata->hModule == NULL)
		return rv;

	// チューナクローズ
	if(tdata->tfd){
		tdata->pIBon->CloseTuner();
		tdata->tfd = FALSE;
	}
	// インスタンス解放 & モジュールリリース
	tdata->pIBon->Release();
	dlclose(tdata->hModule);
	tdata->hModule = NULL;

	return rv;
}


void
calc_cn(thread_data *tdata, boolean use_bell)
{
	double	CNR = (double)tdata->pIBon->GetSignalLevel();

	if(use_bell) {
		int bell = 0;

		if(CNR >= 30.0)
			bell = 3;
		else if(CNR >= 15.0 && CNR < 30.0)
			bell = 2;
		else if(CNR < 15.0)
			bell = 1;
		fprintf(stderr, "\rC/N = %fdB ", CNR);
		do_bell(bell);
	}
	else {
		fprintf(stderr, "\rC/N = %fdB", CNR);
	}
}

void
show_channels(void)
{
	fprintf(stderr, "Available Channels:\n");
	fprintf(stderr, "B0-:          BonDriver Channels\n");
	fprintf(stderr, "1-62:         Terrestrial Channels\n");
	fprintf(stderr, "C13-C63:      CATV Channels\n");
	fprintf(stderr, "BS1_0-BS23_2: BS Channels\n");
	fprintf(stderr, "CS2-CS24:     CS Channels\n");
}

int
parse_time(const char * rectimestr, int *recsec)
{
	/* indefinite */
	if(!strcmp("-", rectimestr)) {
		*recsec = -1;
		return 0;
	}
	/* colon */
	else if(strchr(rectimestr, ':')) {
		int n1, n2, n3;
		if(sscanf(rectimestr, "%d:%d:%d", &n1, &n2, &n3) == 3)
			*recsec = n1 * 3600 + n2 * 60 + n3;
		else if(sscanf(rectimestr, "%d:%d", &n1, &n2) == 2)
			*recsec = n1 * 3600 + n2 * 60;
		else
			return 1; /* unsuccessful */

		return 0;
	}
	/* HMS */
	else {
		char *tmpstr;
		char *p1, *p2;
		int  flag;

		if( *rectimestr == '-' ){
			rectimestr++;
			flag = 1;
		}else
			flag = 0;
		tmpstr = strdup(rectimestr);
		p1 = tmpstr;
		while(*p1 && !isdigit(*p1))
			p1++;

		/* hour */
		if((p2 = strchr(p1, 'H')) || (p2 = strchr(p1, 'h'))) {
			*p2 = '\0';
			*recsec += atoi(p1) * 3600;
			p1 = p2 + 1;
			while(*p1 && !isdigit(*p1))
				p1++;
		}

		/* minute */
		if((p2 = strchr(p1, 'M')) || (p2 = strchr(p1, 'm'))) {
			*p2 = '\0';
			*recsec += atoi(p1) * 60;
			p1 = p2 + 1;
			while(*p1 && !isdigit(*p1))
				p1++;
		}

		/* second */
		*recsec += atoi(p1);
		if( flag )
			*recsec *= -1;

		free(tmpstr);

		return 0;
	} /* else */

	return 1; /* unsuccessful */
}

void
do_bell(int bell)
{
	int i;
	for(i=0; i < bell; i++) {
		fprintf(stderr, "\a");
		usleep(400000);
	}
}

int
tune(char *channel, thread_data *tdata, char *driver)
{
	/* get channel */
	BON_CHANNEL_SET *channel_set = searchrecoff(tdata->dwSpace, channel);
	if(channel_set == NULL) {
		fprintf(stderr, "Invalid Channel: %s\n", channel);
		return 1;
	}
	DWORD dwSendBonNum;
	boolean reqChannel;
	fprintf(stderr, "Space = %d\n", channel_set->bon_space);
	if(channel_set->bon_num != ARIB_CH_ERROR){
		dwSendBonNum = channel_set->bon_num;
		reqChannel = FALSE;
		fprintf(stderr, "Channel = B%d\n", channel_set->bon_num);
	}else{
		dwSendBonNum = channel_set->set_freq;
		reqChannel = TRUE;
		fprintf(stderr, "Channel = %dkHz\n", channel_set->set_freq);
	}

	/* open tuner */
	char *dri_tmp = driver;
	int aera;
	char **tuner;
	int num_devs;
	if(dri_tmp && *dri_tmp == 'P'){
		// proxy
		dri_tmp++;
		aera = 1;
	}else
		aera = 0;
	if(dri_tmp && *dri_tmp != '\0') {
		/* case 1: specified tuner driver */
		int num = 0;
		int code;

		if(*dri_tmp == 'S'){
			if(channel_set->type != CHTYPE_GROUND){
				if(aera == 0){
					tuner = bsdev;
					num_devs = NUM_BSDEV;
				}else{
					tuner = bsdev_proxy;
					num_devs = NUM_BSDEV_PROXY;
				}
			}else
				goto OPEN_TUNER;
		}else if(*dri_tmp == 'T'){
			if(channel_set->type != CHTYPE_SATELLITE){
				if(aera == 0){
					tuner = isdb_t_dev;
					num_devs = NUM_ISDB_T_DEV;
				}else{
					tuner = isdb_t_dev_proxy;
					num_devs = NUM_ISDB_T_DEV_PROXY;
				}
			}else
				goto OPEN_TUNER;
		}else
			goto OPEN_TUNER;
		if(!isdigit(*++dri_tmp))
			goto OPEN_TUNER;
		do{
			num = num * 10 + *dri_tmp++ - '0';
		}while(isdigit(*dri_tmp));
		if(*dri_tmp == '\0' && num+1 <= num_devs)
			driver = tuner[num];
OPEN_TUNER:;
		if((code = open_tuner(tdata, driver))){
			if(code == -4)
				fprintf(stderr, "OpenTuner error: %s\n", driver);
			return 1;
		}
		/* tune to specified channel */
		while(tdata->pIBon2->SetChannel(channel_set->bon_space, dwSendBonNum) == FALSE) {
			if(tdata->tune_persistent) {
				if(f_exit)
					goto err;
				fprintf(stderr, "No signal. Still trying: %s\n", driver);
			}
			else {
				fprintf(stderr, "Cannot tune to the specified channel: %s\n", driver);
				goto err;
			}
		}
		fprintf(stderr, "driver = %s\n", driver);
	}
	else {
		/* case 2: loop around available devices */
		int lp;

		switch(channel_set->type){
			case CHTYPE_BonNUMBER:
			default:
				fprintf(stderr, "No driver name\n");
				goto err;
			case CHTYPE_SATELLITE:
				if(aera == 0){
					tuner = bsdev;
					num_devs = NUM_BSDEV;
				}else{
					tuner = bsdev_proxy;
					num_devs = NUM_BSDEV_PROXY;
				}
				break;
			case CHTYPE_GROUND:
				if(aera == 0){
					tuner = isdb_t_dev;
					num_devs = NUM_ISDB_T_DEV;
				}else{
					tuner = isdb_t_dev_proxy;
					num_devs = NUM_ISDB_T_DEV_PROXY;
				}
				break;
		}

		for(lp = 0; lp < num_devs; lp++) {
			if(open_tuner(tdata, tuner[lp]) == 0) {
				// 同CHチェック
				DWORD m_dwSpace = tdata->pIBon2->GetCurSpace();
				DWORD m_dwChannel = tdata->pIBon2->GetCurChannel();
				if(m_dwSpace == channel_set->bon_space && m_dwChannel == dwSendBonNum)
					goto SUCCESS_EXIT;
				else{
					close_tuner(tdata);
					continue;
				}
			}
		}
		for(lp = 0; lp < num_devs; lp++) {
			int count = 0;

			if(open_tuner(tdata, tuner[lp]) == 0) {
				// 使用中チェック・BSのCh比較は、局再編があると正しくない可能性がある
				DWORD m_dwSpace = tdata->pIBon2->GetCurSpace();
				DWORD m_dwChannel = tdata->pIBon2->GetCurChannel();
				if(m_dwChannel != ARIB_CH_ERROR){
					if(m_dwSpace != channel_set->bon_space || m_dwChannel != dwSendBonNum){
						close_tuner(tdata);
						continue;
					}
				}else{
					/* tune to specified channel */
					if(tdata->tune_persistent) {
						while(tdata->pIBon2->SetChannel(channel_set->bon_space, dwSendBonNum) == FALSE && count < MAX_RETRY) {
							if(f_exit)
								goto err;
							fprintf(stderr, "No signal. Still trying: %s\n", tuner[lp]);
							count++;
						}

						if(count >= MAX_RETRY) {
							close_tuner(tdata);
							count = 0;
							continue;
						}
					} /* tune_persistent */
					else {
						if(tdata->pIBon2->SetChannel(channel_set->bon_space, dwSendBonNum) == FALSE) {
							close_tuner(tdata);
							continue;
						}
					}
				}

				goto SUCCESS_EXIT; /* found suitable tuner */
			}
		}

		/* all tuners cannot be used */
		fprintf(stderr, "Cannot tune to the specified channel\n");
		return 1;

SUCCESS_EXIT:
		if(tdata->tune_persistent)
			fprintf(stderr, "driver = %s\n", tuner[lp]);
	}
	tdata->table = channel_set;
	if(reqChannel) {
		channel_set->bon_space = tdata->pIBon2->GetCurSpace();
		channel_set->bon_num = tdata->pIBon2->GetCurChannel();
	}
	// TS受信開始待ち
	timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 50 * 1000 * 1000;	// 50ms
	{
	int lop = 0;
	do{
		nanosleep(&ts, NULL);
	}while(tdata->pIBon->GetReadyCount() < 2 && ++lop < 20);
	}

	if(!tdata->tune_persistent) {
		/* show signal strength */
		calc_cn(tdata, FALSE);
	}

	return 0; /* success */
err:
	close_tuner(tdata);

	return 1;
}
