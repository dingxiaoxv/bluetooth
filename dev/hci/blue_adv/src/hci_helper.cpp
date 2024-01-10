#include "hci_helper.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include "bluez/bluetooth.h"
#include "bluez/hci.h"
#include "bluez/hci_lib.h"
#include <signal.h>

#define FLAGS_AD_TYPE 0x01
#define FLAGS_LIMITED_MODE_BIT 0x01
#define FLAGS_GENERAL_MODE_BIT 0x02

#define EIR_FLAGS                   0x01  /* flags */
#define EIR_UUID16_SOME             0x02  /* 16-bit UUID, more available */
#define EIR_UUID16_ALL              0x03  /* 16-bit UUID, all listed */
#define EIR_UUID32_SOME             0x04  /* 32-bit UUID, more available */
#define EIR_UUID32_ALL              0x05  /* 32-bit UUID, all listed */
#define EIR_UUID128_SOME            0x06  /* 128-bit UUID, more available */
#define EIR_UUID128_ALL             0x07  /* 128-bit UUID, all listed */
#define EIR_NAME_SHORT              0x08  /* shortened local name */
#define EIR_NAME_COMPLETE           0x09  /* complete local name */
#define EIR_TX_POWER                0x0A  /* transmit power level */
#define EIR_DEVICE_ID               0x10  /* device ID */

static volatile int signal_received = 0;

static void sigint_handler(int sig) {
	signal_received = sig;
}

static int read_flags(uint8_t *flags, const uint8_t *data, size_t size) {
	size_t offset;

	if (!flags || !data)
		return -EINVAL;

	offset = 0;
	while (offset < size) {
		uint8_t len = data[offset];
		uint8_t type;

		/* Check if it is the end of the significant part */
		if (len == 0)
			break;

		if (len + offset > size)
			break;

		type = data[offset + 1];

		if (type == FLAGS_AD_TYPE) {
			*flags = data[offset + 2];
			return 0;
		}

		offset += 1 + len;
	}

	return -ENOENT;
}

static int check_report_filter(uint8_t procedure, le_advertising_info *info) {
	uint8_t flags;

	/* If no discovery procedure is set, all reports are treat as valid */
	if (procedure == 0)
		return 1;

	/* Read flags AD type value from the advertising report if it exists */
	if (read_flags(&flags, info->data, info->length))
		return 0;

	switch (procedure) {
	case 'l': /* Limited Discovery Procedure */
		if (flags & FLAGS_LIMITED_MODE_BIT)
			return 1;
		break;
	case 'g': /* General Discovery Procedure */
		if (flags & (FLAGS_LIMITED_MODE_BIT | FLAGS_GENERAL_MODE_BIT))
			return 1;
		break;
	default:
		fprintf(stderr, "Unknown discovery procedure\n");
	}

	return 0;
}

static void eir_parse_name(uint8_t *eir, size_t eir_len, char *buf, size_t buf_len) {
	size_t offset;

	offset = 0;
	while (offset < eir_len) {
		uint8_t field_len = eir[0];
		size_t name_len;

		/* Check for the end of EIR */
		if (field_len == 0)
			break;

		if (offset + field_len > eir_len)
			goto failed;

		switch (eir[1]) {
		case EIR_NAME_SHORT:
		case EIR_NAME_COMPLETE:
			name_len = field_len - 1;
			if (name_len > buf_len)
				goto failed;

			memcpy(buf, &eir[2], name_len);
			return;
		}

		offset += field_len + 1;
		eir += field_len + 1;
	}

failed:
	snprintf(buf, buf_len, "(unknown)");
}

static int print_advertising_devices(int dd, uint8_t filter_type) {
	unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
	struct hci_filter nf, of;
	struct sigaction sa;
	socklen_t olen;
	int len;

	olen = sizeof(of);
	if (getsockopt(dd, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
		printf("Could not get socket options\n");
		return -1;
	}

	hci_filter_clear(&nf);
	hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
	hci_filter_set_event(EVT_LE_META_EVENT, &nf);

	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
		printf("Could not set socket options\n");
		return -1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = sigint_handler;
	sigaction(SIGINT, &sa, NULL);

	while (1) {
		evt_le_meta_event *meta;
		le_advertising_info *info;
		char addr[18];

		while ((len = read(dd, buf, sizeof(buf))) < 0) {
			if (errno == EINTR && signal_received == SIGINT) {
				len = 0;
				goto done;
			}

			if (errno == EAGAIN || errno == EINTR)
				continue;
			goto done;
		}

		ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
		len -= (1 + HCI_EVENT_HDR_SIZE);

		meta = (evt_le_meta_event *) ptr;

		if (meta->subevent != 0x02)
			goto done;

		/* Ignoring multiple reports */
		info = (le_advertising_info *) (meta->data + 1);
		if (check_report_filter(filter_type, info)) {
			char name[30];

			memset(name, 0, sizeof(name));

			ba2str(&info->bdaddr, addr);
			eir_parse_name(info->data, info->length,
							name, sizeof(name) - 1);

			printf("%s %s\n", addr, name);
		}
	}

done:
	setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

	if (len < 0)
		return -1;

	return 0;
}

HciHelper::HciHelper() : fd_(-1) {
  dev_ = hci_get_route(NULL);
  if (dev_ < 0) {
    std::cerr << "failed to get the device id, error: " << strerror(errno);
    return;
  }

  fd_ = hci_open_dev(dev_);
  if (fd_ < 0) {
    std::cerr << "failed to open device, error: " << strerror(errno);
    return;
  }
}

HciHelper::~HciHelper() {
  if (fd_ >= 0) {
    std::cout << "close hcihelper..." << std::endl;
    hci_close_dev(fd_);
  }
}

HCI_VERSION HciHelper::getHciVersion() {
  struct hci_version ver;
  int ret = hci_read_local_version(fd_, &ver, 1000);
  if (ret < 0) {
    std::cerr << "failed to read local hci version, error: " << strerror(errno);
    return HCI_BT_UNKNOWN;
  }

  if (ver.hci_ver < HCI_BT_4_0 || ver.hci_ver > HCI_BT_5_3) {
    std::cerr << "unsupported hci version: " << ver.hci_ver;
    return HCI_BT_UNKNOWN;
  }

  return (HCI_VERSION) ver.hci_ver;
}

void HciHelper::setLeAdvertisingData(uint16_t service, const std::string &deviceName) {
  struct hci_request rq;
  uint8_t status;
  rq.ogf = OGF_LE_CTL;

  /*  Length: 31
      Flags: 0x02
      LE General Discoverable Mode
      16-bit Service UUIDs (partial): 1 entry
      Tesla Motor Inc. (0xfe97)
      Name (complete): p2102xxxxaax
  */
  LeAdvertising adv;
  adv.service.uuid[0] = service & 0xff;
  adv.service.uuid[1] = (service >> 8) & 0xff;
  memset(adv.localName.name, 0, 22);
  memcpy(adv.localName.name, deviceName.data(), deviceName.size());
  rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  rq.cparam = &adv;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, error = " << strerror(errno);
    return;
  }

  /*  Min advertising interval: 100.000 msec (0x00a0)
      Max advertising interval: 200.000 msec (0x0140)
      Type: Connectable undirected - ADV_IND (0x00)
      Own address type: Public (0x00)
      Direct address type: Public (0x00)
      Direct address: 00:00:00:00:00:00 (OUI 00-00-00)
      Channel map: 37, 38, 39 (0x07)
      Filter policy: Allow Scan Request from Any, Allow Connect Request from Any (0x00)
  */
  rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
  rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
  uint8_t buf[LE_SET_ADVERTISING_PARAMETERS_CP_SIZE] = { 
    0xA0, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00
  };
  rq.cparam = buf;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, ctrl code = 0x06, error = " << strerror(errno);
    return;
  }

  // Advertising: Enabled (0x01)
  rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
  rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
  uint8_t enable = 0x01;
  rq.cparam = &enable;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, ctrl code = 0x0a, error = " << strerror(errno);
    return;
  }
}

void HciHelper::setLeAdvertisingDataExt(uint16_t service, const char* data) {
  struct hci_request rq;
  uint8_t status;
  rq.ogf = OGF_LE_CTL;
  rq.ocf = 0x36;
  rq.clen = 25;
  uint8_t buf[25] = { 
    0x01, 0x13, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08,
    0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7f, 0x01, 0x00, 0x01, 0x00,
    0x00
  };

  rq.cparam = buf;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, ctrl code = 0x36, error = " << strerror(errno);
    return;
  }


  rq.ocf = 0x38;
  rq.clen = 4;
  uint8_t buf2[4] = { 0x01, 0x03, 0x01, 0x00 };
  rq.cparam = buf2;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req, ctrl code = 0x38, error = " << strerror(errno);
    return;
  }


  LeAdvertisingEnable enable;
  enable.enable = 0x01;
  rq.ogf = OGF_LE_CTL;
  rq.ocf = 0x39;
  rq.cparam = &enable;
  rq.clen = 6;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req when enable LE ext adv, error = " << strerror(errno);
    return;
  }

  LeAdvertisingExt adv;
  adv.serviceData.uuid[0] = service & 0xff;
  adv.serviceData.uuid[1] = (service >> 8) & 0xff;
  memcpy(adv.serviceData.data, data, 20);

  rq.ocf = 0x37;
  rq.cparam = &adv;
  rq.clen = 31;
  rq.rparam = &status;
  rq.rlen = 1;

  if (hci_send_req(fd_, &rq, 1000) < 0) {
    std::cerr << "failed to send req when set LE ext adv data, error = " << strerror(errno);
    return;
  }
}

std::string HciHelper::getMacAddress() {
  struct hci_dev_info dev_info;
  memset(&dev_info, 0, sizeof(dev_info));
  dev_info.dev_id = dev_;
  if (hci_devinfo(dev_, &dev_info) < 0) {
    std::cerr << "failed to get device info, error: " << strerror(errno);
    return "00:00:00:00:00:00";
  }

  char addr[18] = { 0 };
  ba2str(&dev_info.bdaddr, addr);
  return addr;
}