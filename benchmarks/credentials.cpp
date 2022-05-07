#include <iostream>

extern "C" {
#include "rdmacred.h"
}

void assert_z(const std::string &text, const int x) {
	if (x != 0) {
    std::cout << "[ERROR] " << text << " failed with code " << x << "\n" << std::endl;
    exit(1);
  }
}

void assert_nEOF(const std::string &text, const int x) {
	if (x == EOF) {
    std::cout << "[ERROR] " << text << " returned EOF\n" << std::endl;
    exit(1);
  }
}

void assert_nNULL(const std::string &text, const void *x)  {
	if (x == NULL) {
    std::cout << "[ERROR] " << text << " returned NULL\n" << std::endl;
    exit(1);
  }
}

int main() {
  // Acquire, grand access and save the credential
  uint32_t credential;
  int ret = drc_acquire(&credential, 0);
  if (ret == 0) {
    char buffer[11];
    FILE *file;
    drc_grant(credential, 28487, DRC_FLAGS_TARGET_UID);
    snprintf(buffer, 11, "%u", credential);
    assert_nNULL("fopen", file = fopen("credential.txt", "w"));
    assert_nEOF("fputs", fputs(buffer, file));
    assert_nEOF("fclose", fclose(file));
    printf("Saved credential %s\n", buffer);
  } else {
    std::cout << "[ERROR] Cannot acquire the credential, failed with code" << ret << std::endl;
    exit(1);
  }
    
	// Access the credential and print cookies
  uint32_t cookie1;
	uint32_t cookie2;
  drc_info_handle_t info;
  uint8_t ptag;
	assert_z("drc_access", drc_access(credential, 0, &info));
	cookie1 = drc_get_first_cookie(info);
  cookie2 = drc_get_second_cookie(info);
  GNI_GetPtag(0, cookie1, &ptag);
	std::cout << "[INFO] Got cookies " << cookie1 << " and " << cookie2 << " with ptag " << ptag << std::endl;
}
