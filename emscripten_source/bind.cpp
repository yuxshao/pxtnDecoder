#include <stdlib.h>
#include <string.h>

#include <emscripten/bind.h>

#include <pxtnDescriptor.h>

#include <pxtoneNoise.h>
#include <pxtnService.h>

using namespace emscripten;

// Pxtone Noise
bool decodeNoise(uintptr_t noise_c, int noise_length, int ch, int sps, int bps, uintptr_t wave_c, uintptr_t wave_length_c) {

	void *		noise 		= (void *) noise_c;
	void **		wave		= (void **) wave_c; 
	int *		wave_length	= (int *) wave_length_c;

	bool			b_ret	= false;
	pxtnDescriptor* doc		= new pxtnDescriptor();
	pxtoneNoise *	pxNoise	= new pxtoneNoise();

	void **		buffer;

	// set buffer to doc
	if(!doc->set_memory_r(noise, noise_length))			goto End;

	// create noise
	if(!pxNoise->init())								goto End;
	if(!pxNoise->quality_set(ch, sps, bps))				goto End;

	// set doc to noise
	if(!pxNoise->generate(doc, buffer, wave_length))	goto End;

	// memcpy
	*wave = malloc(*wave_length);
	memcpy(*wave, *buffer, *wave_length);

	b_ret = true;

End:
	if(pxNoise)		delete pxNoise;
	if(doc)			delete doc;
	if(*wave && !b_ret)	free(*wave);

	return b_ret;
}

// new pxtone version only supports a fixed bps
pxtnERR check_bps(int bps) {
	if(bps != pxtnBITPERSAMPLE) {
		printf("ERROR: bps must be %d\n", pxtnBITPERSAMPLE);
		return pxtnERR_INIT;
	}
	return pxtnOK;
}

// Pxtone Project
bool createPxtone(uintptr_t pxtn_c, int pxtn_length, int ch, int sps, int bps, uintptr_t pxVomit_c, uintptr_t doc_c) {

	void *		pxtn		= (void *) pxtn_c;

	void **		pxVomit_m	= (void **)	pxVomit_c;
	void **		doc_m		= (void **)	doc_c;

	bool		b_ret		= false;	

	pxtnDescriptor *	doc	= new pxtnDescriptor();
	pxtnService *	pxVomit	= new pxtnService();
	
	// set buffer to doc
	pxtnERR        pxtn_err = pxtnERR_VOID;
	if(!doc->set_memory_r(pxtn, pxtn_length))		goto End;

	// create vomit
	pxtn_err = pxVomit->init(); if( pxtn_err != pxtnOK )	goto End;
	if(!pxVomit->set_destination_quality(ch, sps))			goto End;
	pxtn_err = check_bps(bps); if( pxtn_err != pxtnOK )		goto End;

	// set doc to vomit
	pxtn_err = pxVomit->read(doc);		if( pxtn_err != pxtnOK ) goto End;
	pxtn_err = pxVomit->tones_ready();	if( pxtn_err != pxtnOK ) goto End;

	*pxVomit_m = (void *) pxVomit;
	*doc_m = (void *) doc;

	b_ret = true;

End:
	if(!b_ret) {
		delete pxVomit;
		delete doc;

		printf("ERROR: pxtnERR[ %s ]\n", pxtnError_get_string( pxtn_err ) );
	}

	return b_ret;
}

bool getPxtoneText(uintptr_t pxVomit_c, uintptr_t title_c, uintptr_t title_length_c, uintptr_t comment_c, uintptr_t comment_length_c) {

	void **		pxVomit_m	= (void **)	pxVomit_c;

	void **		title			= (void **) title_c;
	int *		title_length	= (int *) title_length_c;
	void **		comment			= (void **) comment_c;
	int *		comment_length	= (int *) comment_length_c;

	pxtnService *		pxVomit	= (pxtnService *) *pxVomit_m;

	// title, comment
	*title = (void *) pxVomit->text->get_name_buf(title_length);
	*comment = (void *) pxVomit->text->get_comment_buf(comment_length);

	return true;
}

bool getPxtoneInfo(uintptr_t pxVomit_c, int ch, int sps, int bps, uintptr_t wave_length_c, uintptr_t loopStart_c, uintptr_t loopEnd_c) {

	void **		pxVomit_m	= (void **)	pxVomit_c;

	int *		wave_length	= (int *) wave_length_c;
	double *	loopStart	= (double *) loopStart_c;
	double *	loopEnd		= (double *) loopEnd_c;

	bool		b_ret		= false;

	pxtnService * pxVomit	= (pxtnService *) *pxVomit_m;

	int			beatNum	;
	float		beatTempo;
	int			measNum;
	int			sampleNum;
	double		duration;

	pxVomit->master->Get(&beatNum, &beatTempo, NULL, &measNum);
	if( check_bps(bps) != pxtnOK ) goto End;
	sampleNum = pxtnService_moo_CalcSampleNum(measNum, beatNum, sps, beatTempo) * ch * bps / 8;

	// length
	*wave_length = sampleNum;

	// loop
	duration = (double)sampleNum / (double)ch / ((double)bps / 8) / (double)sps;
	*loopStart	= (double)pxVomit->master->get_repeat_meas() / (double)measNum * duration;
	*loopEnd	= (double)pxVomit->master->get_play_meas() / (double)measNum * duration;

	// vomit start
	{
		pxtnVOMITPREPARATION prep = {0};
		prep.flags          |= pxtnVOMITPREPFLAG_loop;
		prep.start_pos_float =   0;
		prep.master_volume   = 1.f;
		if(!pxVomit->moo_preparation(&prep)) goto End;
	}

	b_ret = true;

End:
	return b_ret;
}

// getPxtoneMaster and getPxtoneInfo get a lot of the same data, but getPxtonInfo gets
// durations for audio playback, and getPxtoneMaster gets counts for drawing calculations.
bool getPxtoneMaster(uintptr_t pxServ_c,
	uintptr_t beatNum, uintptr_t beatTempo, uintptr_t beatClock, uintptr_t measNum,
    uintptr_t repeatMeas, uintptr_t lastMeas) {
	void **		pxServ_m = (void **) pxServ_c;
	pxtnService *pxtn	 = (pxtnService *) *pxServ_m;

	pxtn->master->Get((int *)beatNum, (float *)beatTempo, (int *)beatClock, (int *)measNum);

    *((int *)repeatMeas) = pxtn->master->get_repeat_meas();
    *((int *)lastMeas) = pxtn->master->get_last_meas();

	return true;
}

bool getPxtoneUnits(uintptr_t pxServ_c, uintptr_t unitNum_c,
		uintptr_t names_c, uintptr_t sizes_c) {
	void **		pxServ_m = (void **) pxServ_c;
	pxtnService *pxtn	 = (pxtnService *) *pxServ_m;

	int32_t *		unitNum	= (int *) unitNum_c;
	int32_t **		sizes	= (int **) sizes_c;
	const char ***	names	= (const char ***) names_c;

	*unitNum = pxtn->Unit_Num();

	*sizes	= (int *)malloc(*unitNum * sizeof(int));
	*names	= (const char **)malloc(*unitNum * sizeof(char *));
	for (int i = 0; i < *unitNum; ++i) {
		(*names)[i] = pxtn->Unit_Get(i)->get_name_buf((*sizes) + i);
		// sizes from get_name_buf are upper bounds. js postprocessing does not
		// detect null character so we do it here
		if ((*sizes)[i] > strlen((*names)[i])) {
			(*sizes)[i] = strlen((*names)[i]);
		}
	}
	return true;
}

bool getPxtoneEvels(uintptr_t pxServ_c, uintptr_t evelNum_c,
		uintptr_t kinds_c, uintptr_t units_c, uintptr_t values_c, uintptr_t clocks_c) {
	void **		pxServ_m = (void **) pxServ_c;
	pxtnService *pxtn	 = (pxtnService *) *pxServ_m;

	int *		evelNum	= (int *) evelNum_c;

	uint8_t **	kinds	= (uint8_t **) kinds_c;
	uint8_t **	units	= (uint8_t **) units_c;
	int32_t **	values	= (int32_t **) values_c;
	int32_t **	clocks	= (int32_t **) clocks_c;

	*evelNum = pxtn->evels->get_Count();
	*kinds	= (uint8_t *)malloc(*evelNum * sizeof(uint8_t));
	*units	= (uint8_t *)malloc(*evelNum * sizeof(uint8_t));
	*values	= (int32_t *)malloc(*evelNum * sizeof(int32_t));
	*clocks	= (int32_t *)malloc(*evelNum * sizeof(int32_t));

	int i = 0;
	for (const EVERECORD *p = pxtn->evels->get_Records(); p; p = p->next, ++i) {
		(*kinds)[i]	 = p->kind;
		(*units)[i]	 = p->unit_no;
		(*values)[i] = p->value;
		(*clocks)[i] = p->clock;
	}

	return true;
}

bool vomitPxtone(uintptr_t pxVomit_c, uintptr_t buffer_c, int size) {

	void **		pxVomit_m	= (void **)	pxVomit_c;
	void *		buffer		= (void *)	buffer_c;

	pxtnService *	pxVomit	= (pxtnService *) *pxVomit_m;

	bool		b_ret			= false;

	if(!pxVomit->Moo(buffer, size))	goto End;
	
	b_ret = true;
End:
	return b_ret;
}

void releasePxtone(uintptr_t pxVomit_c, uintptr_t doc_c) {

	void **		pxVomit_m	= (void **)	pxVomit_c;
	void **		doc_m		= (void **)	doc_c;

	pxtnService *	pxVomit	= (pxtnService *) *pxVomit_m;
	pxtnDescriptor *doc		= (pxtnDescriptor *) *doc_m;

	if(pxVomit)	delete pxVomit;
	if(doc)		delete doc;
}


EMSCRIPTEN_BINDINGS(px_module) {
	function("decodeNoise", &decodeNoise);
	function("createPxtone", &createPxtone);
	function("releasePxtone", &releasePxtone);
	function("getPxtoneText", &getPxtoneText);
	function("getPxtoneInfo", &getPxtoneInfo);
	function("getPxtoneMaster", &getPxtoneMaster);
	function("getPxtoneUnits", &getPxtoneUnits);
	function("getPxtoneEvels", &getPxtoneEvels);
	function("vomitPxtone", &vomitPxtone);
}
