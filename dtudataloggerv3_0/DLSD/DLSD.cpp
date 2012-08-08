#include <Arduino.h>
#include "DLSD.h"

prog_char sd_filename_0[] PROGMEM = "CONFIG";
prog_char sd_filename_1[] PROGMEM = "DAT00000";
prog_char sd_filename_2[] PROGMEM = "SYS00000";
prog_char sd_filename_3[] PROGMEM = "SER00000";

PROGMEM const char *sd_filename_table[] = {
					   sd_filename_0, sd_filename_1,
                                           sd_filename_2, sd_filename_3, 
					   sd_filename_1 // Read only for previous log file
					  };

prog_char sd_filename_ext[] PROGMEM = ".DAT";

DLSD::DLSD(char fullspeed, uint8_t CS)
{
	_CS = CS;
	_inited = 0;
	_fullspeed = fullspeed;
	for(uint8_t i = 0; i < NUM_FILES; i++)
		_files_count[i] = 0;
}

int8_t DLSD::init() {
	uint8_t ret = 0;
//	if (_fullspeed)
//		ret = _card.init(SPI_FULL_SPEED, _CS);
//	else
/*	ret = _card.init(_fullspeed, _CS);
	if (!ret) { // Failed to initialize card
		_inited = -1;
		return -1;
	}

	if (!Fat16::init(&_card)) { // Failed to initialize volume
		_inited = -2;
		return -2;
	}
*/
	if (!_card.begin(_CS, _fullspeed)) {
		_inited = -1;
		return -1;
	}
	if (_DEBUG)
		_card.ls(LS_DATE | LS_SIZE);
	_inited = 1;
	return 1;
}

void DLSD::error() {
	_card.errorPrint();
}

uint8_t DLSD::get_num_files() {
	return NUM_FILES;
}

uint16_t DLSD::get_files_count(uint8_t fid) {
	return _files_count[fid];
}

uint8_t DLSD::set_files_count(uint8_t fid, uint16_t count) {
	_files_count[fid] = count;
	return 1;
}

void DLSD::reset_files_count() {
	for(uint8_t i = 0; i < NUM_FILES; i++)
		_files_count[i] = 0;
}

uint16_t DLSD::get_saved_count(uint8_t fid) {
	return _saved_count[fid];
}

uint8_t DLSD::set_saved_count(uint8_t fid, uint16_t count) {
	_saved_count[fid] = count;
	return 1;
}

void DLSD::reset_saved_count() {
	for(uint8_t i=0;i < NUM_FILES; i++)
		_saved_count[i] = 0;
}

void DLSD::seek_forward_files_count() {
	int n, cnt;
	bool exists = true;
	for(n=FILES_CNT_START;n<=FILES_CNT_END;n++) {
		exists = true;
		cnt = _files_count[n];
		do {
			*_filename = '\0';
			get_from_flash(&(sd_filename_table[n]), _filename);
			if (n != 0)
				pad_filename(_filename, cnt);
			strcat_P(_filename, sd_filename_ext);
			exists = _card.exists(_filename);
			cnt++;
		} while (exists);
		Serial.print("Setting #");
		Serial.print(n, DEC);
		Serial.print(" to ");
		Serial.println(cnt, DEC);
		set_files_count(n, cnt);
	}
}

int8_t DLSD::is_available() {
	return _inited;
}

void DLSD::pad_filename(char *filename, uint16_t c) {
	char nums[6];
	//itoa(c, nums, 10);
	fmtUnsigned(c, nums, 10);
	uint8_t a = strlen(nums);
	for(char i=0;i<a;i++) {
		filename[8-(a-i)] = nums[i];
	}	
}

bool DLSD::increment_file(uint8_t n) {
	close(n);
	_files_count[n] = _files_count[n] + 1;
	return true;
}

void DLSD::debug(int v){
	_DEBUG = v;
}

unsigned long DLSD::open(uint8_t n, uint8_t flags) {
	uint8_t ret;
	unsigned long fsize = 0;
	digitalWrite(_CS, LOW);
	if (_files_open[n] == false) {
		*_filename = '\0';
		get_from_flash(&(sd_filename_table[n]), _filename);
		if (n != 0)
			pad_filename(_filename, _files_count[n]);
		strcat_P(_filename, sd_filename_ext);		
		if (_DEBUG) {
			Serial.print("Opening ");
			Serial.println(_filename);
		}
		ret = _files[n].open(_filename, flags);
		if (!ret || !_files[n].isOpen())
			return -1;
		_files_open[n] = true;
		fsize = _files[n].fileSize();
	} else {
		fsize = _files[n].fileSize();
	}
	return fsize;
}

bool DLSD::close(uint8_t n) {
	_files[n].sync();
	if (!_files[n].close())
		return false;
	_files_open[n] = false;
	digitalWrite(_CS, HIGH);
	return true;
}

bool DLSD::write(uint8_t n, char *ptr) {
	_files[n].clearWriteError();
	_files[n].print(ptr);
	_files[n].sync();
	return _files[n].writeError;
}
		
bool DLSD::write(uint8_t n, float a) {
	_files[n].clearWriteError();
	_files[n].print(a);
	_files[n].sync();
	return _files[n].writeError;
}

bool DLSD::write(uint8_t n, unsigned short a) {
        _files[n].clearWriteError();
	_files[n].print(a);
	_files[n].sync();
	return _files[n].writeError;
}

bool DLSD::write(uint8_t n, int a) {
        _files[n].clearWriteError();
	_files[n].print(a);
	_files[n].sync();
	return _files[n].writeError;
}

bool DLSD::write(uint8_t n, unsigned long a) {
        _files[n].clearWriteError();	
	_files[n].print(a);
	_files[n].sync();
	return _files[n].writeError;
}

int DLSD::read(uint8_t n, char *ptr, int len) {
	return _files[n].read(ptr, len);
}

int DLSD::read(uint8_t n, char *ptr, int len, char t) {
	int c = 0;
	while (c < len) {
		ptr[c] = _files[n].read();
		if (ptr[c] < 0 || ptr[c] == t) {
			if (ptr[c] < 0) {
				ptr[c] = 0;
				return -1;
			}
			else {
				ptr[c] = 0;
				return 0;
			}
		}
		c++;
	}
	ptr[c] = 0;
	return 1;
}

void DLSD::rewind(uint8_t n) {
	_files[n].rewind();
}

bool DLSD::seek(uint8_t n, uint32_t pos) {
	_files[n].seekSet(pos);
}
                
bool DLSD::seekend(uint8_t n) {
	_files[n].seekEnd();
}

bool DLSD::exists(char *fname) {
	return _card.exists(fname);	
}

uint8_t DLSD::copy(char *src, char *dst) {
  char buf[512];
  SdFile file1, file2;
  
  if (!file1.open(src, O_READ)) {
    error();
  }

  if (!file2.open(dst, O_WRITE | O_CREAT | O_TRUNC)) {
    error();
  }

  // copy file1 to file2
  file1.rewind();

  while (1) {
    int n = file1.read(buf, sizeof(buf));
    if (n < 0) error();
    if (n == 0) break;
    if (file2.write(buf, n) != n) error();
  }
  file1.sync();
  file2.sync();
  file1.close();
  file2.close();
  return 1;
}
