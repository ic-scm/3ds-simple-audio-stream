//Simple 3DS audio file streaming example
//Written by I.C. - released to public domain

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include <3ds.h>

#define SAMPLERATE 48000

//Buffer size
#define SAMPLESPERBUF 4096

//Bytes per sample, in this case it is 4 bytes to fit a 2-byte (16-bit) sample for both channels.
#define BYTESPERSAMPLE 4

void fill_buffer(void *audioBuffer, size_t size, int16_t* buffer) {
    u32 *dest = (u32*)audioBuffer;
    
    for (int i=0; i<size; i++) {
        
        int16_t sample = buffer[i];
        
        dest[i] = (sample<<16) | (sample & 0xffff);
    }
    
    DSP_FlushDataCache(audioBuffer, size);
}

int main(int argc, char **argv) {
    //Default ctrulib and text console settings
    PrintConsole topScreen;
    gfxInitDefault();
    consoleInit(GFX_TOP, &topScreen);
    consoleSelect(&topScreen);
    
    //Initialize RomFS
    romfsInit();
    
    printf("libctru audio streaming from file\n");
    printf("Example program written by I.C. - \nreleased to public domain\n\n");
    
    //NDSP double audio buffer
    ndspWaveBuf waveBuf[2];
    
    u32 *audioBuffer = (u32*)linearAlloc(SAMPLESPERBUF*BYTESPERSAMPLE*2);
    
    //The currently used block from the double buffer.
    bool fillBlock = false;
    
    //Initialize NDSP
    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SAMPLERATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
    
    //Set buffer settings
    memset(waveBuf,0,sizeof(waveBuf));
    waveBuf[0].data_vaddr = &audioBuffer[0];
    waveBuf[0].nsamples = SAMPLESPERBUF;
    waveBuf[1].data_vaddr = &audioBuffer[SAMPLESPERBUF];
    waveBuf[1].nsamples = SAMPLESPERBUF;
    
    //Add the empty buffers to NDSP to initialize default values in the buffer structs.
    ndspChnWaveBufAdd(0, &waveBuf[0]);
    ndspChnWaveBufAdd(0, &waveBuf[1]);
    
    //Open our audio file
    FILE *file = fopen("romfs:/audio.bin", "rb");
    
    off_t file_size;
    char* file_buffer;
    
    if(file == NULL) {
        printf("Error - the file could not be opened. %s\n", strerror(errno));
    } else {
        
        fseek(file, 0, SEEK_END);
        file_size = ftell(file);
        
        fseek(file, 0, SEEK_SET);
        file_buffer = (char*)malloc(file_size);
        
        off_t bytesRead = fread(file_buffer, 1, file_size, file);
        
        if(ferror(file) || file_buffer == NULL) {
            printf("Error - the file could not be loaded. %s\n", strerror(errno));
        }
        
        fclose(file);
        
    }
    
    //Playback head position in the file (in samples).
    unsigned long position = 0;
    
    //Smaller audio buffer of input file data, in signed 16-bit format
    int16_t* file_audio_buffer = (int16_t*)malloc(SAMPLESPERBUF * sizeof(int16_t));
    
    if(file_audio_buffer == NULL) {
        printf("Error - the audio buffer could not be allocated.\n");
    }
    
    printf("Press START to exit.\n");
    
    while(aptMainLoop()) {
        
        gfxSwapBuffers();
        gfxFlushBuffers();
        gspWaitForVBlank();
        
        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown & KEY_START) break; // break in order to return to hbmenu
        
        if (waveBuf[fillBlock].status == NDSP_WBUF_DONE) {
            
            //Get a small part of the audio in the file into the audio buffer.
            for(unsigned int i=0; i < SAMPLESPERBUF; i++) {
                // ----------------------| Convert the file data stored in char* to a signed 16-bit sample.
                file_audio_buffer[i] = ( file_buffer[position * 2] | file_buffer[position * 2 + 1] << 8);
                
                //Increment position in file, and reset it if we reached end of file.
                position = (position + 1) % (file_size / 2);
            }
            
            fill_buffer(waveBuf[fillBlock].data_pcm16, waveBuf[fillBlock].nsamples, file_audio_buffer);
            
            ndspChnWaveBufAdd(0, &waveBuf[fillBlock]);
            
            fillBlock = !fillBlock;
        }
    }
    
    ndspExit();
    
    linearFree(audioBuffer);
    free(file_buffer);
    free(file_audio_buffer);
    
    gfxExit();
    romfsExit();
    
    return 0;
}
