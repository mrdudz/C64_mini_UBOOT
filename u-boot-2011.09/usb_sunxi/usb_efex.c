/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include "usb_base.h"
#include <scsi.h>
#include <asm/arch/dma.h>
#include <sys_partition.h>
#include <sys_config.h>
#include <sprite.h>
#include <boot_type.h>
#include "usb_efex.h"


#define  SUNXI_USB_EFEX_IDLE					 (0)
#define  SUNXI_USB_EFEX_SETUP					 (1)
#define  SUNXI_USB_EFEX_SEND_DATA				 (2)
#define  SUNXI_USB_EFEX_RECEIVE_DATA			 (3)
#define  SUNXI_USB_EFEX_STATUS					 (4)
#define  SUNXI_USB_EFEX_EXIT					 (5)

#define  SUNXI_USB_EFEX_APPS_MAST				 (0xf0000)

#define  SUNXI_USB_EFEX_APPS_IDLE				 (0x10000)
#define  SUNXI_USB_EFEX_APPS_CMD				 (0x20000)
#define  SUNXI_USB_EFEX_APPS_DATA				 (0x30000)
#define  SUNXI_USB_EFEX_APPS_SEND_DATA		     (SUNXI_USB_EFEX_APPS_DATA | SUNXI_USB_EFEX_SEND_DATA)
#define  SUNXI_USB_EFEX_APPS_RECEIVE_DATA		 (SUNXI_USB_EFEX_APPS_DATA | SUNXI_USB_EFEX_RECEIVE_DATA)
#define  SUNXI_USB_EFEX_APPS_STATUS				 ((0x40000)  | SUNXI_USB_EFEX_STATUS)
#define  SUNXI_USB_EFEX_APPS_EXIT				 ((0x50000)  | SUNXI_USB_EFEX_EXIT)

#define DRIVER_VENDOR_ID			0x1F3A
#define DRIVER_PRODUCT_ID			0xEfE8

static  int sunxi_usb_efex_write_enable = 0;
static  int sunxi_usb_efex_status = SUNXI_USB_EFEX_IDLE;
static  int sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_IDLE;
static  efex_trans_set_t  trans_data;
static  u8  *cmd_buf;
static  u32 sunxi_efex_next_action = 0;
#if defined(SUNXI_USB_30)
static  int sunxi_usb_efex_status_enable = 1;
#endif
/*
*******************************************************************************
*                     do_usb_req_set_interface
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static int __usb_set_interface(struct usb_device_request *req)
{
	sunxi_usb_dbg("set interface\n");
	/* Only support interface 0, alternate 0 */
	if((0 == req->wIndex) && (0 == req->wValue))
	{
		sunxi_udc_ep_reset();
	}
	else
	{
		printf("err: invalid wIndex and wValue, (0, %d), (0, %d)\n", req->wIndex, req->wValue);
		return SUNXI_USB_REQ_OP_ERR;
	}

	return SUNXI_USB_REQ_SUCCESSED;
}

/*
*******************************************************************************
*                     do_usb_req_set_address
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static int __usb_set_address(struct usb_device_request *req)
{
	uchar address;

	address = req->wValue & 0x7f;
	printf("set address 0x%x\n", address);

	sunxi_udc_set_address(address);

	return SUNXI_USB_REQ_SUCCESSED;
}

/*
*******************************************************************************
*                     do_usb_req_set_configuration
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static int __usb_set_configuration(struct usb_device_request *req)
{
	sunxi_usb_dbg("set configuration\n");
	/* Only support 1 configuration so nak anything else */
	if (1 == req->wValue)
	{
		sunxi_udc_ep_reset();
	}
	else
	{
		printf("err: invalid wValue, (0, %d)\n", req->wValue);

		return SUNXI_USB_REQ_OP_ERR;
	}

	sunxi_udc_set_configuration(req->wValue);

	return SUNXI_USB_REQ_SUCCESSED;
}
/*
*******************************************************************************
*                     do_usb_req_get_descriptor
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static int __usb_get_descriptor(struct usb_device_request *req, uchar *buffer)
{
	int ret = SUNXI_USB_REQ_SUCCESSED;

	//��ȡ������
	switch(req->wValue >> 8)
	{
		case USB_DT_DEVICE:		//�豸������
		{
			struct usb_device_descriptor *dev_dscrptr;

			sunxi_usb_dbg("get device descriptor\n");

			dev_dscrptr = (struct usb_device_descriptor *)buffer;
			memset((void *)dev_dscrptr, 0, sizeof(struct usb_device_descriptor));

			dev_dscrptr->bLength = MIN(req->wLength, sizeof (struct usb_device_descriptor));
			dev_dscrptr->bDescriptorType    = USB_DT_DEVICE;
#ifdef CONFIG_USB_1_1_DEVICE
			dev_dscrptr->bcdUSB             = 0x110;
#else
			dev_dscrptr->bcdUSB             = 0x200;
#endif
			dev_dscrptr->bDeviceClass       = 0;
			dev_dscrptr->bDeviceSubClass    = 0;
			dev_dscrptr->bDeviceProtocol    = 0;
			dev_dscrptr->bMaxPacketSize0    = 0x40;
			dev_dscrptr->idVendor           = DRIVER_VENDOR_ID;
			dev_dscrptr->idProduct          = DRIVER_PRODUCT_ID;
			dev_dscrptr->bcdDevice          = 0xffff;
			//ignored
			//dev_dscrptr->iManufacturer      = SUNXI_USB_STRING_IMANUFACTURER;
			//dev_dscrptr->iProduct           = SUNXI_USB_STRING_IPRODUCT;
			//dev_dscrptr->iSerialNumber      = SUNXI_USB_STRING_ISERIALNUMBER;
			dev_dscrptr->iManufacturer      = 0;
			dev_dscrptr->iProduct           = 0;
			dev_dscrptr->iSerialNumber      = 0;
			dev_dscrptr->bNumConfigurations = 1;

			sunxi_udc_send_setup(dev_dscrptr->bLength, buffer);
		}
		break;

		case USB_DT_CONFIG:		//����������
		{
			struct usb_configuration_descriptor *config_dscrptr;
			struct usb_interface_descriptor 	*inter_dscrptr;
			struct usb_endpoint_descriptor 		*ep_in, *ep_out;
			unsigned char bytes_remaining = req->wLength;
			unsigned char bytes_total = 0;

			sunxi_usb_dbg("get config descriptor\n");

			bytes_total = sizeof (struct usb_configuration_descriptor) + \
						  sizeof (struct usb_interface_descriptor) 	   + \
						  sizeof (struct usb_endpoint_descriptor) 	   + \
						  sizeof (struct usb_endpoint_descriptor);

			memset(buffer, 0, bytes_total);

			config_dscrptr = (struct usb_configuration_descriptor *)(buffer + 0);
			inter_dscrptr  = (struct usb_interface_descriptor 	  *)(buffer + 						\
																	 sizeof(struct usb_configuration_descriptor));
			ep_in 		   = (struct usb_endpoint_descriptor 	  *)(buffer + 						\
																	 sizeof(struct usb_configuration_descriptor) + 	\
																	 sizeof(struct usb_interface_descriptor));
			ep_out 		   = (struct usb_endpoint_descriptor 	  *)(buffer + 						\
																	 sizeof(struct usb_configuration_descriptor) + 	\
																	 sizeof(struct usb_interface_descriptor)	 +	\
																	 sizeof(struct usb_endpoint_descriptor));

			/* configuration */
			config_dscrptr->bLength            	= MIN(bytes_remaining, sizeof (struct usb_configuration_descriptor));
			config_dscrptr->bDescriptorType    	= USB_DT_CONFIG;
			config_dscrptr->wTotalLength 		= bytes_total;
			config_dscrptr->bNumInterfaces     	= 1;
			config_dscrptr->bConfigurationValue	= 1;
			config_dscrptr->iConfiguration     	= 0;
			config_dscrptr->bmAttributes       	= 0x80;		//not self powered
			config_dscrptr->bMaxPower          	= 0xFA;		//������500ms(0xfa * 2)

			bytes_remaining 				   -= config_dscrptr->bLength;
			/* interface */
			inter_dscrptr->bLength             = MIN (bytes_remaining, sizeof(struct usb_interface_descriptor));
			inter_dscrptr->bDescriptorType     = USB_DT_INTERFACE;
			inter_dscrptr->bInterfaceNumber    = 0x00;
			inter_dscrptr->bAlternateSetting   = 0x00;
			inter_dscrptr->bNumEndpoints       = 0x02;
			inter_dscrptr->bInterfaceClass     = 0xff;
			inter_dscrptr->bInterfaceSubClass  = 0xff;
			inter_dscrptr->bInterfaceProtocol  = 0xff;
			inter_dscrptr->iInterface          = 0;

			bytes_remaining 				  -= inter_dscrptr->bLength;
			/* ep_in */
			ep_in->bLength            = MIN (bytes_remaining, sizeof (struct usb_endpoint_descriptor));
			ep_in->bDescriptorType    = USB_DT_ENDPOINT;
			ep_in->bEndpointAddress   = sunxi_udc_get_ep_in_type(); /* IN */
			ep_in->bmAttributes       = USB_ENDPOINT_XFER_BULK;
			ep_in->wMaxPacketSize 	  = sunxi_udc_get_ep_max();
			ep_in->bInterval          = 0x00;

			bytes_remaining 		 -= ep_in->bLength;
			/* ep_out */
			ep_out->bLength            = MIN (bytes_remaining, sizeof (struct usb_endpoint_descriptor));
			ep_out->bDescriptorType    = USB_DT_ENDPOINT;
			ep_out->bEndpointAddress   = sunxi_udc_get_ep_out_type(); /* OUT */
			ep_out->bmAttributes       = USB_ENDPOINT_XFER_BULK;
			ep_out->wMaxPacketSize 	   = sunxi_udc_get_ep_max();
			ep_out->bInterval          = 0x00;

			bytes_remaining 		  -= ep_out->bLength;

			sunxi_udc_send_setup(MIN(req->wLength, bytes_total), buffer);
		}
		break;

		case USB_DT_STRING:
		{
			unsigned char bLength = 0;
			unsigned char string_index = req->wValue & 0xff;

			sunxi_usb_dbg("get string descriptor\n");

			/* Language ID */
			if(string_index == 0)
			{
				bLength = MIN(4, req->wLength);

				buffer[0] = bLength;
				buffer[1] = USB_DT_STRING;
				buffer[2] = 9;
				buffer[3] = 4;
				sunxi_udc_send_setup(bLength, (void *)buffer);
			}
			else
			{
				printf("sunxi usb err: string line %d is not supported\n", string_index);
			}
		}
		break;

		case USB_DT_DEVICE_QUALIFIER:
		{
#ifdef CONFIG_USB_1_1_DEVICE
			/* This is an invalid request for usb 1.1, nak it */
			USBC_Dev_EpSendStall(sunxi_udc_source.usbc_hd, USBC_EP_TYPE_EP0);
#else
			struct usb_qualifier_descriptor *qua_dscrpt;

			sunxi_usb_dbg("get qualifier descriptor\n");

			qua_dscrpt = (struct usb_qualifier_descriptor *)buffer;
			memset(&buffer, 0, sizeof(struct usb_qualifier_descriptor));

			qua_dscrpt->bLength = MIN(req->wLength, sizeof(sizeof(struct usb_qualifier_descriptor)));
			qua_dscrpt->bDescriptorType    = USB_DT_DEVICE_QUALIFIER;
			qua_dscrpt->bcdUSB             = 0x200;
			qua_dscrpt->bDeviceClass       = 0xff;
			qua_dscrpt->bDeviceSubClass    = 0xff;
			qua_dscrpt->bDeviceProtocol    = 0xff;
			qua_dscrpt->bMaxPacketSize0    = 0x40;
			qua_dscrpt->bNumConfigurations = 1;
			qua_dscrpt->bRESERVED          = 0;

			sunxi_udc_send_setup(qua_dscrpt->bLength, buffer);
#endif
		}
		break;

		default:
			printf("err: unkown wValue(%d)\n", req->wValue);

			ret = SUNXI_USB_REQ_OP_ERR;
	}

	return ret;
}

/*
*******************************************************************************
*                     do_usb_req_get_status
*
* Description:
*    void
*
* Parameters:
*    void
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
static int __usb_get_status(struct usb_device_request *req, uchar *buffer)
{
	unsigned char bLength = 0;

	sunxi_usb_dbg("get status\n");
	if(0 == req->wLength)
	{
		/* sent zero packet */
		sunxi_udc_send_setup(0, NULL);

		return SUNXI_USB_REQ_OP_ERR;
	}

	bLength = MIN(req->wValue, 2);

	buffer[0] = 1;
	buffer[1] = 0;

	sunxi_udc_send_setup(bLength, buffer);

	return SUNXI_USB_REQ_SUCCESSED;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static int __sunxi_efex_send_status(void *buffer, unsigned int buffer_size)
{
	return sunxi_udc_send_data((uchar *)buffer, buffer_size);
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static int sunxi_efex_init(void)
{
	sunxi_usb_dbg("sunxi_efex_init\n");
	memset(&trans_data, 0, sizeof(efex_trans_set_t));
	sunxi_usb_efex_write_enable = 0;
    sunxi_usb_efex_status = SUNXI_USB_EFEX_IDLE;
    sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_IDLE;

	cmd_buf = (u8 *)malloc(CBW_MAX_CMD_SIZE);
	if(!cmd_buf)
	{
		printf("sunxi usb efex err: unable to malloc memory for cmd\n");

		return -1;
	}
    trans_data.base_recv_buffer = (u8 *)malloc(SUNXI_EFEX_RECV_MEM_SIZE);
    if(!trans_data.base_recv_buffer)
    {
    	printf("sunxi usb efex err: unable to malloc memory for efex receive\n");
		free(cmd_buf);

    	return -1;
    }

	trans_data.base_send_buffer = (u8 *)malloc(SUNXI_EFEX_RECV_MEM_SIZE);
    if(!trans_data.base_send_buffer)
    {
    	printf("sunxi usb efex err: unable to malloc memory for efex send\n");
    	free(trans_data.base_recv_buffer);
		free(cmd_buf);

    	return -1;
    }
	sunxi_usb_dbg("recv addr 0x%x\n", (uint)trans_data.base_recv_buffer);
    sunxi_usb_dbg("send addr 0x%x\n", (uint)trans_data.base_send_buffer);

    return 0;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static int sunxi_efex_exit(void)
{
	sunxi_usb_dbg("sunxi_efex_exit\n");
    if(trans_data.base_recv_buffer)
    {
    	free(trans_data.base_recv_buffer);
    }
    if(trans_data.base_send_buffer)
    {
    	free(trans_data.base_send_buffer);
    }
    if(cmd_buf)
    {
    	free(cmd_buf);
	}

    return 0;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static void sunxi_efex_reset(void)
{
	sunxi_usb_efex_write_enable = 0;
    sunxi_usb_efex_status = SUNXI_USB_EFEX_IDLE;
    sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_IDLE;
    trans_data.to_be_recved_size = 0;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static void  sunxi_efex_usb_rx_dma_isr(void *p_arg)
{
	sunxi_usb_dbg("dma int for usb rx occur\n");
	//֪ͨ��ѭ��������д������
	sunxi_usb_efex_write_enable = 1;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static void  sunxi_efex_usb_tx_dma_isr(void *p_arg)
{
	sunxi_usb_dbg("dma int for usb tx occur\n");

#if defined(SUNXI_USB_30)
	sunxi_usb_efex_status_enable ++;
#endif
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static int sunxi_efex_standard_req_op(uint cmd, struct usb_device_request *req, uchar *buffer)
{
	int ret = SUNXI_USB_REQ_OP_ERR;

	switch(cmd)
	{
		case USB_REQ_GET_STATUS:
		{
			ret = __usb_get_status(req, buffer);

			break;
		}
		//case USB_REQ_CLEAR_FEATURE:
		//case USB_REQ_SET_FEATURE:
		case USB_REQ_SET_ADDRESS:
		{
			ret = __usb_set_address(req);

			break;
		}
		case USB_REQ_GET_DESCRIPTOR:
		//case USB_REQ_SET_DESCRIPTOR:
		case USB_REQ_GET_CONFIGURATION:
		{
			ret = __usb_get_descriptor(req, buffer);

			break;
		}
		case USB_REQ_SET_CONFIGURATION:
		{
			ret = __usb_set_configuration(req);

			break;
		}
		//case USB_REQ_GET_INTERFACE:
		case USB_REQ_SET_INTERFACE:
		{
			ret = __usb_set_interface(req);

			break;
		}
		//case USB_REQ_SYNCH_FRAME:
		default:
		{
			printf("sunxi efex error: standard req is not supported\n");

			ret = SUNXI_USB_REQ_DEVICE_NOT_SUPPORTED;

			break;
		}
	}

	return ret;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static int sunxi_efex_nonstandard_req_op(uint cmd, struct usb_device_request *req, uchar *buffer, uint data_status)
{
	int ret = SUNXI_USB_REQ_SUCCESSED;

	switch(req->bmRequestType)
	{
		case 161:
			if(req->bRequest == 0xFE)
			{
				sunxi_usb_dbg("efex ask for max lun\n");

				buffer[0] = 0;

				sunxi_udc_send_setup(1, buffer);
			}
			else
			{
				printf("sunxi usb err: unknown ep0 req in efex\n");

				ret = SUNXI_USB_REQ_DEVICE_NOT_SUPPORTED;
			}
			break;

		default:
			printf("sunxi usb err: unknown non standard ep0 req\n");

			ret = SUNXI_USB_REQ_DEVICE_NOT_SUPPORTED;

			break;
	}

	return ret;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static void __sunxi_usb_efex_fill_status(void)
{
	Status_t     *efex_status;

	efex_status = (Status_t *)trans_data.base_send_buffer;
	memset(efex_status, 0, sizeof(Status_t));
	efex_status->mark  = 0xffff;
	efex_status->tag   = 0;
	efex_status->state = 0;

	trans_data.act_send_buffer = (uint)trans_data.base_send_buffer;
	trans_data.send_size = sizeof(Status_t);

	return;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static int __sunxi_usb_efex_op_cmd(u8 *cmd_buffer)
{
	struct global_cmd_s  *cmd = (struct global_cmd_s *)cmd_buffer;

	switch(cmd->app_cmd)
	{
		case APP_LAYER_COMMEN_CMD_VERIFY_DEV:
			sunxi_usb_dbg("APP_LAYER_COMMEN_CMD_VERIFY_DEV\n");
			{
				struct verify_dev_data_s *app_verify_dev;

				app_verify_dev = (struct verify_dev_data_s *)trans_data.base_send_buffer;

				memcpy(app_verify_dev->tag, AL_VERIFY_DEV_TAG_DATA, sizeof(AL_VERIFY_DEV_TAG_DATA));
				app_verify_dev->platform_id_hw 		= FES_PLATFORM_HW_ID;
				app_verify_dev->platform_id_fw 		= 0x0001;
				app_verify_dev->mode 				= AL_VERIFY_DEV_MODE_SRV;//�̶��ģ�
				app_verify_dev->pho_data_flag 		= 'D';
				app_verify_dev->pho_data_len 		= PHOENIX_PRIV_DATA_LEN_NR;
				app_verify_dev->pho_data_start_addr = PHOENIX_PRIV_DATA_ADDR;

				trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
				trans_data.send_size         = sizeof(struct verify_dev_data_s);
				trans_data.last_err          = 0;
				trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;
  			}

			break;

		case APP_LAYER_COMMEN_CMD_SWITCH_ROLE:
			sunxi_usb_dbg("APP_LAYER_COMMEN_CMD_SWITCH_ROLE\n");
			sunxi_usb_dbg("not supported\n");

			trans_data.last_err          = -1;
			trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_STATUS;

			break;

		case APP_LAYER_COMMEN_CMD_IS_READY:
			sunxi_usb_dbg("APP_LAYER_COMMEN_CMD_IS_READY\n");

			{
				struct is_ready_data_s *app_is_ready_data;

				app_is_ready_data = (struct is_ready_data_s *)trans_data.base_send_buffer;

				app_is_ready_data->interval_ms = 500;
				app_is_ready_data->state = AL_IS_READY_STATE_READY;

				trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
				trans_data.send_size         = sizeof(struct is_ready_data_s);
				trans_data.last_err          = 0;
				trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;
			}

			break;

		case APP_LAYER_COMMEN_CMD_GET_CMD_SET_VER:
			sunxi_usb_dbg("APP_LAYER_COMMEN_CMD_GET_CMD_SET_VER\n");

			{
				struct get_cmd_set_ver_data_s *app_get_cmd_ver_data;

				app_get_cmd_ver_data = (struct get_cmd_set_ver_data_s *)trans_data.base_send_buffer;

				app_get_cmd_ver_data->ver_high = 1;
				app_get_cmd_ver_data->ver_low = 0;

				trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
				trans_data.send_size         = sizeof(struct get_cmd_set_ver_data_s);
				trans_data.last_err          = 0;
				trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;
			}

			break;

		case APP_LAYER_COMMEN_CMD_DISCONNECT:
			sunxi_usb_dbg("APP_LAYER_COMMEN_CMD_DISCONNECT\n");
			sunxi_usb_dbg("not supported\n");

			trans_data.last_err          = -1;
			trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_STATUS;

			break;

		case FEX_CMD_fes_trans:
			sunxi_usb_dbg("FEX_CMD_fes_trans\n");

			//��Ҫ��������
			{
				fes_trans_old_t  *fes_old_data = (fes_trans_old_t *)cmd_buf;

				if(fes_old_data->len)
				{
					if(fes_old_data->u2.DOU == 2)		//�ϴ�����
					{
#ifdef SUNXI_USB_DEBUG
						uint value;

						value = *(uint *)fes_old_data->addr;
#endif
						sunxi_usb_dbg("send id 0x%x, addr 0x%x, length 0x%x\n", value, fes_old_data->addr, fes_old_data->len);
						trans_data.act_send_buffer   = fes_old_data->addr;	//���÷��͵�ַ
						trans_data.send_size         = fes_old_data->len;	//���÷��ͳ���
						trans_data.last_err          = 0;
						trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;
					}
					else	//(fes_old_data->u2.DOU == (0 or 1))	//��������
					{
#ifdef SUNXI_USB_DEBUG
						uint value;

						value = *(uint *)fes_old_data->addr;
#endif
						sunxi_usb_dbg("receive id 0x%x, addr 0x%x, length 0x%x\n", value, fes_old_data->addr, fes_old_data->len);

						trans_data.type = SUNXI_EFEX_DRAM_TAG;		//д��dram������
						trans_data.act_recv_buffer   = fes_old_data->addr;	//���ý��յ�ַ
						trans_data.recv_size         = fes_old_data->len;	//���ý��ճ���
						trans_data.last_err          = 0;
						trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_RECEIVE_DATA;
					}
				}
				else
				{
					printf("FEX_CMD_fes_trans: no data need to send or receive\n");

					trans_data.app_next_status = SUNXI_USB_EFEX_APPS_STATUS;
				}
			}
			trans_data.last_err = 0;

			break;

		case FEX_CMD_fes_run:
			sunxi_usb_dbg("FEX_CMD_fes_run\n");
			{
#ifdef  CONFIG_CMD_ELF
				fes_run_t   *runs = (fes_run_t *)cmd_buf;
				int         *app_ret;
				char run_addr[32] = {0};
				char paras[8][16];
				char *const  usb_runs_args[12] = {NULL, run_addr, 					\
					                            (char *)&paras[0][0], 				\
					                            (char *)&paras[1][0],				\
					                            (char *)&paras[2][0],				\
					                            (char *)&paras[3][0],				\
					                            (char *)&paras[4][0],				\
					                            (char *)&paras[5][0],				\
					                            (char *)&paras[6][0],				\
					                        	(char *)&paras[7][0]};

				sprintf(run_addr, "%x", runs->addr);
				printf("usb run addr      = %s\n", run_addr);
				printf("usb run paras max = %d\n", runs->max_para);
				{
					int i;
					int *data;

					data = runs->para_addr;
					for(i=0;i<runs->max_para;i++)
					{
						printf("usb run paras[%d] = 0x%x\n", i, data[i]);
						sprintf((char *)&paras[i][0], "%x", data[i]);
					}
				}

				app_ret = (int *)trans_data.base_send_buffer;
				*app_ret = do_bootelf(NULL, 0, runs->max_para + 1, usb_runs_args);
				printf("usb get result = %d\n", *app_ret);
				trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
				trans_data.send_size         = 4;
				trans_data.last_err          = 0;
				trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;
#else
				int         *app_ret;

				app_ret = (int *)trans_data.base_send_buffer;
				*app_ret = -1;
				trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
				trans_data.send_size         = 4;
				trans_data.last_err          = 0;
				trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;
#endif
			}

			break;

		case FEX_CMD_fes_down:
			sunxi_usb_dbg("FEX_CMD_fes_down\n");
			{
				fes_trans_t  *trans = (fes_trans_t *)cmd_buf;

				trans_data.type  = trans->type;									 //�������ͣ�MBR,BOOT1,BOOT0...�Լ���������
				if((trans->type & SUNXI_EFEX_DRAM_MASK) == SUNXI_EFEX_DRAM_MASK) //��������ڴ����ݣ���ִ������
				{
					if((SUNXI_EFEX_DRAM_MASK | SUNXI_EFEX_TRANS_FINISH_TAG) == trans->type)
					{
					    trans_data.act_recv_buffer = (uint)trans_data.base_recv_buffer;
						trans_data.dram_trans_buffer = trans->addr;
						//printf("dram write: start 0x%x: length 0x%x\n", trans->addr, trans->len);
					}
					else
					{
						trans_data.act_recv_buffer   = (uint)trans_data.base_recv_buffer + trans_data.to_be_recved_size;	 //���ý��յ�ַ
					}
					trans_data.recv_size         = trans->len;	//���ý��ճ��ȣ��ֽڵ�λ
					trans_data.to_be_recved_size += trans->len;
					//printf("down dram: start 0x%x, sectors 0x%x\n", trans_data.flash_start, trans_data.flash_sectors);
				}
				else	//����flash���ݣ��ֱ��ʾ��ʼ������������
				{
					trans_data.act_recv_buffer   = (uint)(trans_data.base_recv_buffer + SUNXI_EFEX_RECV_MEM_SIZE/2);	 //���ý��յ�ַ
					trans_data.recv_size         = trans->len;	//���ý��ճ��ȣ��ֽڵ�λ

					trans_data.flash_start       = trans->addr;
					trans_data.flash_sectors     = (trans->len + 511) >> 9;
					//printf("down flash: start 0x%x, sectors 0x%x\n", trans_data.flash_start, trans_data.flash_sectors);
				}
				trans_data.last_err          = 0;
				trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_RECEIVE_DATA;
			}

			break;
		case FEX_CMD_fes_up:
			sunxi_usb_dbg("FEX_CMD_fes_up\n");

			{
				fes_trans_t  *trans = (fes_trans_t *)cmd_buf;

				trans_data.last_err          	 = 0;
				trans_data.app_next_status   	 = SUNXI_USB_EFEX_APPS_SEND_DATA;
				trans_data.type  = trans->type;									 //�������ͣ�MBR,BOOT1,BOOT0...�Լ���������
				if((trans->type & SUNXI_EFEX_DRAM_MASK) == SUNXI_EFEX_DRAM_MASK) //��������ڴ����ݣ���ִ������
				{
					if((SUNXI_EFEX_DRAM_MASK | SUNXI_EFEX_TRANS_FINISH_TAG) == trans->type)
					{
					    trans_data.act_send_buffer = (uint)trans_data.base_send_buffer;
						trans_data.dram_trans_buffer = trans->addr;
						//printf("dram write: start 0x%x: length 0x%x\n", trans->addr, trans->len);
					}
					else
					{
						trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer + trans_data.send_size;	 //���÷������ݵ�ַ
					}

					trans_data.act_send_buffer   = trans->addr;	//���÷��͵�ַ�������ֽڵ�λ
					trans_data.send_size        += trans->len;	//���ý��ճ��ȣ��ֽڵ�λ
					//printf("dram read: start 0x%x: length 0x%x\n", trans->addr, trans->len);
				}
				else	//����flash���ݣ��ֱ��ʾ��ʼ������������
				{
					trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;	 //���÷��͵�ַ
					trans_data.send_size         = trans->len;	//���ý��ճ��ȣ��ֽڵ�λ

					trans_data.flash_start       = trans->addr; //���÷��͵�ַ������������λ
					trans_data.flash_sectors     = (trans->len + 511) >> 9;

					sunxi_usb_dbg("upload flash: start 0x%x, sectors 0x%x\n", trans_data.flash_start, trans_data.flash_sectors);
					if(!sunxi_sprite_read(trans_data.flash_start, trans_data.flash_sectors, (void *)trans_data.act_send_buffer))
					{
						printf("flash read err: start 0x%x, sectors 0x%x\n", trans_data.flash_start, trans_data.flash_sectors);

						trans_data.last_err      = -1;
					}
				}
				//trans_data.last_err          	 = 0;
				//trans_data.app_next_status   	 = SUNXI_USB_EFEX_APPS_SEND_DATA;
			}

			break;

//		case FEX_CMD_fes_verify:
//			printf("FEX_CMD_fes_verify\n");
//			{
//				fes_cmd_verify_t  *cmd_verify = (fes_cmd_verify_t *)cmd_buf;
//				fes_efex_verify_t *verify_data= (fes_efex_verify_t *)trans_data.base_send_buffer;
//
//				printf("FEX_CMD_fes_verify cmd tag = 0x%x\n", cmd_verify->tag);
//				if(cmd_verify->tag == 0)		//������flash��У��
//				{
//					verify_data->media_crc = sunxi_sprite_part_rawdata_verify(cmd_verify->start, cmd_verify->size);
//				}
//				else							//�������ر����ݵ�У��
//				{
//					verify_data->media_crc = trans_data.last_err;
//				}
//				verify_data->flag = EFEX_CRC32_VALID_FLAG;
//				printf("FEX_CMD_fes_verify last err=%d\n", verify_data->media_crc);
//
//				trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
//				trans_data.send_size         = sizeof(fes_efex_verify_t);
//				trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;
////				//Ŀǰֻ֧��У��ͣ����߲鿴״̬�ķ�ʽ
////				if(data_type == SUNXI_EFEX_MBR_TAG)			//����MBR�Ѿ����
////				{
////					verify_data->flag = EFEX_CRC32_VALID_FLAG;
////				}
////				else if(data_type == SUNXI_EFEX_BOOT1_TAG)	//����BOOT1�Ѿ����
////				{
////					verify_data->flag = EFEX_CRC32_VALID_FLAG;
////				}
////				else if(data_type == SUNXI_EFEX_BOOT0_TAG)	//����BOOT0�Ѿ����
////				{
////					verify_data->flag = EFEX_CRC32_VALID_FLAG;
////				}
////				else										//�������ݣ�ֱ��д���ڴ�
////				{
////					memcpy((void *)trans_data.start, sunxi_ubuf->rx_data_buffer, trans_data.size);
////				}
//			}
//			break;

		case FEX_CMD_fes_verify_value:
			sunxi_usb_dbg("FEX_CMD_fes_verify_value\n");
			{
				fes_cmd_verify_value_t  *cmd_verify = (fes_cmd_verify_value_t *)cmd_buf;
				fes_efex_verify_t 		*verify_data= (fes_efex_verify_t *)trans_data.base_send_buffer;

				verify_data->media_crc = sunxi_sprite_part_rawdata_verify(cmd_verify->start, cmd_verify->size);
				verify_data->flag 	   = EFEX_CRC32_VALID_FLAG;

				printf("FEX_CMD_fes_verify_value, start 0x%x, size high 0x%x:low 0x%x\n", cmd_verify->start, (uint)(cmd_verify->size>>32), (uint)(cmd_verify->size));
				printf("FEX_CMD_fes_verify_value 0x%x\n", verify_data->media_crc);
			}
			trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
			trans_data.send_size         = sizeof(fes_cmd_verify_value_t);
			trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;

			break;
		case FEX_CMD_fes_verify_status:
			printf("FEX_CMD_fes_verify_status\n");
			{
//				fes_cmd_verify_status_t *cmd_verify = (fes_cmd_verify_status_t *)cmd_buf;
				fes_efex_verify_t 		*verify_data= (fes_efex_verify_t *)trans_data.base_send_buffer;

				verify_data->flag 	   = EFEX_CRC32_VALID_FLAG;
				verify_data->media_crc = trans_data.last_err;

				printf("FEX_CMD_fes_verify last err=%d\n", verify_data->media_crc);
			}
			trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
			trans_data.send_size         = sizeof(fes_cmd_verify_status_t);
			trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;

			break;
		case FEX_CMD_fes_query_storage:
			sunxi_usb_dbg("FEX_CMD_fes_query_storage\n");

			{
				uint *storage_type = (uint *)trans_data.base_send_buffer;

				*storage_type = uboot_spare_head.boot_data.storage_type;

				trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
				trans_data.send_size         = 4;
				trans_data.last_err          = 0;
				trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;
			}

			break;

		case FEX_CMD_fes_flash_set_on:
			sunxi_usb_dbg("FEX_CMD_fes_flash_set_on\n");

			trans_data.last_err = sunxi_sprite_init(0);
			trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_STATUS;

			break;

		case FEX_CMD_fes_flash_set_off:
			sunxi_usb_dbg("FEX_CMD_fes_flash_set_off\n");

			trans_data.last_err = sunxi_sprite_exit(1);
			trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_STATUS;

			break;

		case FEX_CMD_fes_flash_size_probe:
			sunxi_usb_dbg("FEX_CMD_fes_flash_size_probe\n");

			{
				uint *flash_size = (uint *)trans_data.base_send_buffer;

				*flash_size = sunxi_sprite_size();
				printf("flash sectors: 0x%x\n", *flash_size);
				trans_data.act_send_buffer   = (uint)trans_data.base_send_buffer;
				trans_data.send_size         = 4;
				trans_data.last_err          = 0;
				trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_SEND_DATA;
			}
			break;

		case FEX_CMD_fes_tool_mode:
			sunxi_usb_dbg("FEX_CMD_fes_tool_mode\n");

			{
				fes_efex_tool_t *fes_work = (fes_efex_tool_t *)cmd_buf;

				if(fes_work->tool_mode== WORK_MODE_USB_TOOL_UPDATE)
				{	//������������ߣ���ֱ������
					if(fes_work->next_mode == 0)
					{
						sunxi_efex_next_action = SUNXI_UPDATE_NEXT_ACTION_REBOOT;
					}
					else
					{
						sunxi_efex_next_action = fes_work->next_mode;
					}
					trans_data.app_next_status = SUNXI_USB_EFEX_APPS_EXIT;
				}
				else  //������������ߣ���������ô���
				{
					if(!fes_work->next_mode)
					{
						if(script_parser_fetch("platform", "next_work", (int *)&sunxi_efex_next_action, 1))
						{	//���û�����ã��򲻴����κ�����
							sunxi_efex_next_action = SUNXI_UPDATE_NEXT_ACTION_NORMAL;
						}
					}
					else
					{
						sunxi_efex_next_action = SUNXI_UPDATE_NEXT_ACTION_REBOOT;
					}
					if((sunxi_efex_next_action <= SUNXI_UPDATE_NEXT_ACTION_NORMAL) || (sunxi_efex_next_action > SUNXI_UPDATE_NEXT_ACTION_REUPDATE))
					{
						sunxi_efex_next_action = SUNXI_UPDATE_NEXT_ACTION_NORMAL;
						trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_STATUS;
					}
					else
					{
						trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_EXIT;
					}
				}
			}
			printf("sunxi_efex_next_action=%d\n", sunxi_efex_next_action);
			trans_data.last_err          = 0;

			break;

		case  FEX_CMD_fes_memset:
		    sunxi_usb_dbg("FEX_CMD_fes_memset\n");
		    {
		        fes_efex_memset_t *fes_memset = (fes_efex_memset_t *)cmd_buf;

		        sunxi_usb_dbg("start 0x%x, value 0x%x, length 0x%x\n", fes_memset->start_addr, fes_memset->value & 0xff, fes_memset->length);
		        memset((void *)fes_memset->start_addr, fes_memset->value & 0xff, fes_memset->length);
		    }
		    trans_data.last_err          = 0;
		    trans_data.app_next_status   = SUNXI_USB_EFEX_APPS_STATUS;

		    break;

		default:
			printf("not supported command 0x%x now\n", cmd->app_cmd);

			trans_data.last_err        = -1;
			trans_data.app_next_status = SUNXI_USB_EFEX_APPS_STATUS;
	}

	return 0;
}
/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
static int sunxi_efex_state_loop(void  *buffer)
{
	static struct sunxi_efex_cbw_t  *cbw;
	static struct sunxi_efex_csw_t   csw;
	sunxi_ubuf_t *sunxi_ubuf = (sunxi_ubuf_t *)buffer;

	switch(sunxi_usb_efex_status)
	{
		case SUNXI_USB_EFEX_IDLE:
			if(sunxi_ubuf->rx_ready_for_data == 1)
			{
				sunxi_usb_efex_status = SUNXI_USB_EFEX_SETUP;
			}

			break;

		case SUNXI_USB_EFEX_SETUP:		//cbw

			sunxi_usb_dbg("SUNXI_USB_EFEX_SETUP\n");

			if(sunxi_ubuf->rx_req_length != sizeof(struct sunxi_efex_cbw_t))
			{
				printf("sunxi usb error: received bytes 0x%x is not equal cbw struct size 0x%x\n", sunxi_ubuf->rx_req_length, sizeof(struct sunxi_efex_cbw_t));

				sunxi_ubuf->rx_ready_for_data = 0;
				sunxi_usb_efex_status = SUNXI_USB_EFEX_IDLE;

				//data length err
				return -1;
			}

			cbw = (struct sunxi_efex_cbw_t *)sunxi_ubuf->rx_req_buffer;
			if(CBW_MAGIC != cbw->magic)
			{
				printf("sunxi usb error: the cbw signature 0x%x is bad, need 0x%x\n", cbw->magic, CBW_MAGIC);

				sunxi_ubuf->rx_ready_for_data = 0;
				sunxi_usb_efex_status = SUNXI_USB_EFEX_IDLE;

				//data value err
				return -1;
			}

			csw.magic = CSW_MAGIC;		//"AWUS"
			csw.tag   = cbw->tag;

#if defined(SUNXI_USB_30)
			sunxi_usb_efex_status_enable = 1;
#endif

			sunxi_usb_dbg("usb cbw trans direction = 0x%x\n", cbw->cmd_package.direction);
			if(sunxi_usb_efex_app_step == SUNXI_USB_EFEX_APPS_IDLE)
			{
				sunxi_usb_dbg("APPS: SUNXI_USB_EFEX_APPS_IDLE\n");
				if(cbw->cmd_package.direction == TL_CMD_RECEIVE)	//С���˽�������
				{
					sunxi_usb_dbg("APPS: SUNXI_USB_EFEX_APPS_IDLE: TL_CMD_RECEIVE\n");
					sunxi_ubuf->request_size = min(cbw->data_transfer_len, CBW_MAX_CMD_SIZE);
					sunxi_usb_dbg("try to receive data 0x%x\n", sunxi_ubuf->request_size);
					sunxi_usb_efex_write_enable = 0;
					if(sunxi_ubuf->request_size)
					{
						sunxi_udc_start_recv_by_dma((uint)cmd_buf, sunxi_ubuf->request_size);	//start dma to receive data
					}
					else
					{
						printf("APPS: SUNXI_USB_EFEX_APPS_IDLE: the send data length is 0\n");

						return -1;
					}
					//��һ�׶ν��յ���������app
					sunxi_usb_efex_status   = SUNXI_USB_EFEX_RECEIVE_DATA;	//����׶Σ���һ�׶ν���������
					sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_CMD;		//����׶Σ���һ�׶ν��յ�������
				}
				else	//setup�׶μ�usb��bulk�����һ�׶Σ�ֻ�ܽ������ݣ����ܷ���
				{
					printf("APPS: SUNXI_USB_EFEX_APPS_IDLE: INVALID direction\n");
					printf("sunxi usb efex app cmd err: usb transfer direction is receive only\n");

					return -1;
				}
			}
			else if((sunxi_usb_efex_app_step == SUNXI_USB_EFEX_APPS_SEND_DATA) ||			\
				(sunxi_usb_efex_app_step == SUNXI_USB_EFEX_APPS_RECEIVE_DATA))	//�յ��ĵڶ��׶Σ���ʱ��ʼ��������
			{
				sunxi_usb_dbg("APPS: SUNXI_USB_EFEX_APPS_DATA\n");
				if(cbw->cmd_package.direction == TL_CMD_RECEIVE)	//���Ҫ�������ݣ�����������dma��ʼ����
				{
					sunxi_usb_dbg("APPS: SUNXI_USB_EFEX_APPS_DATA: TL_CMD_RECEIVE\n");
					sunxi_ubuf->request_size = MIN(cbw->data_transfer_len, trans_data.recv_size);	//���ճ���
					//sunxi_usb_dbg("try to receive data 0x%x\n", sunxi_ubuf->request_size);
					sunxi_usb_efex_write_enable = 0;				//���ñ�־
					if(sunxi_ubuf->request_size)
					{
						sunxi_usb_dbg("dma recv addr = 0x%x\n", trans_data.act_recv_buffer);
						sunxi_udc_start_recv_by_dma(trans_data.act_recv_buffer, sunxi_ubuf->request_size);	//start dma to receive data
					}
					else
					{
						printf("APPS: SUNXI_USB_EFEX_APPS_DATA: the send data length is 0\n");

						return -1;
					}
				}
				//���������������׶ε���һ��״̬
				sunxi_usb_efex_app_step = trans_data.app_next_status;	//���������ȡ��һ����׶�״̬
				//sunxi_usb_dbg("APPS: SUNXI_USB_EFEX_APPS_CMD_DECODE finish\n");
				//sunxi_usb_dbg("sunxi_usb_efex_app_step = 0x%x\n", sunxi_usb_efex_app_step);
				sunxi_usb_efex_status   = sunxi_usb_efex_app_step & 0xffff;						//ʶ�������׶���һ�׶�״̬
																								//�����Ƿ������ݣ��������ݣ�����״̬(csw)
			}
			else if(sunxi_usb_efex_app_step == SUNXI_USB_EFEX_APPS_STATUS)
			{
				sunxi_usb_dbg("APPS: SUNXI_USB_EFEX_APPS_STATUS\n");
				if(cbw->cmd_package.direction == TL_CMD_TRANSMIT)		//��������
				{
					sunxi_usb_dbg("APPS: SUNXI_USB_EFEX_APPS_STATUS: TL_CMD_TRANSMIT\n");
					__sunxi_usb_efex_fill_status();

					sunxi_usb_efex_status = SUNXI_USB_EFEX_SEND_DATA;
					sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_IDLE;
				}
				else	//���һ���׶Σ�ֻ�ܷ������ݣ����ܽ���
				{
					printf("APPS: SUNXI_USB_EFEX_APPS_STATUS: INVALID direction\n");
					printf("sunxi usb efex app status err: usb transfer direction is transmit only\n");
				}
			}
			else if(sunxi_usb_efex_app_step == SUNXI_USB_EFEX_APPS_EXIT)
			{
				sunxi_usb_dbg("APPS: SUNXI_USB_EFEX_APPS_EXIT\n");
				__sunxi_usb_efex_fill_status();

				sunxi_usb_efex_status = SUNXI_USB_EFEX_SEND_DATA;
			}

			break;

	  	case SUNXI_USB_EFEX_SEND_DATA:

	  		sunxi_usb_dbg("SUNXI_USB_EFEX_SEND_DATA\n");
			{
				uint tx_length = MIN(cbw->data_transfer_len, trans_data.send_size);

#if defined(SUNXI_USB_30)
				sunxi_usb_efex_status_enable = 0;
#endif
				sunxi_usb_dbg("send data start 0x%x, size 0x%x\n", trans_data.act_send_buffer, tx_length);
				if(tx_length)
				{
					sunxi_udc_send_data((void *)trans_data.act_send_buffer, tx_length);
				}
				sunxi_usb_efex_status = SUNXI_USB_EFEX_STATUS;
				if(sunxi_usb_efex_app_step == SUNXI_USB_EFEX_APPS_SEND_DATA)//����������׶Σ�Ҫ�������ݣ���һ�׶�ֻ���Ƿ���״̬(status_t)
				{
					sunxi_usb_dbg("SUNXI_USB_EFEX_SEND_DATA next: SUNXI_USB_EFEX_APPS_STATUS\n");
					sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_STATUS;
				}
				else if(sunxi_usb_efex_app_step == SUNXI_USB_EFEX_APPS_EXIT)
				{
					sunxi_usb_dbg("SUNXI_USB_EFEX_SEND_DATA next: SUNXI_USB_EFEX_APPS_EXIT\n");
					sunxi_usb_efex_status = SUNXI_USB_EFEX_EXIT;
					//sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_EXIT;
				}
			}
	  		break;

	  	case SUNXI_USB_EFEX_RECEIVE_DATA:

	  		sunxi_usb_dbg("SUNXI_USB_RECEIVE_DATA\n");
			if(sunxi_usb_efex_write_enable == 1)		//���ݲ��ֽ������
			{
				csw.status = 0;
				//���ֳ������������
				if(sunxi_usb_efex_app_step == SUNXI_USB_EFEX_APPS_CMD)
				{
					//������cmd_buf���´δ���Ҳ��Ҫ
					sunxi_usb_dbg("SUNXI_USB_RECEIVE_DATA: SUNXI_USB_EFEX_APPS_CMD\n");
					if(sunxi_ubuf->request_size != CBW_MAX_CMD_SIZE)		//��������ݣ��򷵻�
					{
						printf("sunxi usb efex err: received cmd size 0x%x is not equal to CBW_MAX_CMD_SIZE 0x%x\n", sunxi_ubuf->request_size, CBW_MAX_CMD_SIZE);

						sunxi_usb_efex_status = SUNXI_USB_EFEX_IDLE;
						csw.status = -1;
					}
					else
					{
						__sunxi_usb_efex_op_cmd(cmd_buf);
						csw.status = trans_data.last_err;
					}
					//sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_DATA;	//����׶Σ����������ɣ���һ�׶δ�������
					sunxi_usb_efex_app_step = trans_data.app_next_status;
				}
				else if(sunxi_usb_efex_app_step == SUNXI_USB_EFEX_APPS_RECEIVE_DATA)//����������׶Σ�Ҫ��������ݣ���һ�׶�ֻ���Ƿ���״̬(status_t)
				{
					//��ʾ���������Ѿ��������
					uint data_type = trans_data.type & SUNXI_EFEX_DATA_TYPE_MASK;

					sunxi_usb_dbg("SUNXI_USB_RECEIVE_DATA: SUNXI_USB_EFEX_APPS_RECEIVE_DATA\n");
					sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_STATUS;
					if(trans_data.type & SUNXI_EFEX_DRAM_MASK)		//��ʾ�����ڴ����ݣ���Ҫ���ȱ��浽�ڴ���
					{
						sunxi_usb_dbg("SUNXI_EFEX_DRAM_MASK\n");
						if(trans_data.type & SUNXI_EFEX_TRANS_FINISH_TAG)	//��ʾ��ǰ���������Ѿ��������
						{
							if(data_type == SUNXI_EFEX_MBR_TAG)			//����MBR�Ѿ����
							{
								//���MBR����ȷ��
								trans_data.last_err = sunxi_sprite_verify_mbr((void *)trans_data.base_recv_buffer);
								if(!trans_data.last_err)
								{
									nand_get_mbr((char *)trans_data.base_recv_buffer, 16 * 1024);
									//׼������
									if(!sunxi_sprite_erase_flash((void *)trans_data.base_recv_buffer))
									{//��¼mbr
										printf("SUNXI_EFEX_MBR_TAG\n");
										printf("mbr size = 0x%x\n", trans_data.to_be_recved_size);
										trans_data.last_err = sunxi_sprite_download_mbr((void *)trans_data.base_recv_buffer, trans_data.to_be_recved_size);
									}
									else
									{
										trans_data.last_err = -1;
									}
								}
							}
							else if(data_type == SUNXI_EFEX_BOOT1_TAG)	//����BOOT1�Ѿ����
							{
								printf("SUNXI_EFEX_BOOT1_TAG\n");
								printf("boot1 size = 0x%x\n", trans_data.to_be_recved_size);
								trans_data.last_err = sunxi_sprite_download_uboot((void *)trans_data.base_recv_buffer, uboot_spare_head.boot_data.storage_type, 0);
							}
							else if(data_type == SUNXI_EFEX_BOOT0_TAG)	//����BOOT0�Ѿ����
							{
								printf("SUNXI_EFEX_BOOT0_TAG\n");
								printf("boot0 size = 0x%x\n", trans_data.to_be_recved_size);
								trans_data.last_err = sunxi_sprite_download_boot0((void *)trans_data.base_recv_buffer, uboot_spare_head.boot_data.storage_type);
							}
							else if(data_type == SUNXI_EFEX_ERASE_TAG)
							{
                                uint erase_flag;

								printf("SUNXI_EFEX_ERASE_TAG\n");
			                    erase_flag = *(uint *)trans_data.base_recv_buffer;
								if(erase_flag)
								{
								    erase_flag = 1;
								}
								printf("erase_flag = 0x%x\n", erase_flag);
								script_parser_patch("platform", "eraseflag", &erase_flag , 1);
							}
                            else//�������ݣ�ֱ��д���ڴ�
							{
                                memcpy((void *)trans_data.dram_trans_buffer, (void *)trans_data.act_recv_buffer, trans_data.recv_size);

								sunxi_usb_dbg("SUNXI_EFEX_DRAM_TAG\n");

								trans_data.last_err = 0;
							}
							trans_data.to_be_recved_size = 0;
						}
						//���ݻ�û�н�����ϣ��ȴ���������
					}
					else		//��ʾ��ǰ������Ҫд��flash
					{
						sunxi_usb_dbg("SUNXI_EFEX_FLASH_MASK\n");
						if(!sunxi_sprite_write(trans_data.flash_start, trans_data.flash_sectors, (void *)trans_data.act_recv_buffer))
						{
							printf("sunxi usb efex err: write flash from 0x%x, sectors 0x%x failed\n", trans_data.flash_start, trans_data.flash_sectors);
							csw.status = -1;
							trans_data.last_err = -1;

							sunxi_usb_efex_app_step = SUNXI_USB_EFEX_APPS_IDLE;
						}
					}
				}
				sunxi_usb_efex_status   = SUNXI_USB_EFEX_STATUS;			//����׶Σ���һ�׶δ���״̬(csw)
			}

			break;

		case SUNXI_USB_EFEX_STATUS:

			sunxi_usb_dbg("SUNXI_USB_EFEX_STATUS\n");
#if defined(SUNXI_USB_30)
			if(sunxi_usb_efex_status_enable)
#endif
			{
				sunxi_usb_efex_status = SUNXI_USB_EFEX_IDLE;

				sunxi_ubuf->rx_ready_for_data = 0;

				__sunxi_efex_send_status(&csw, sizeof(struct sunxi_efex_csw_t));
			}

			break;

		case SUNXI_USB_EFEX_EXIT:
			sunxi_usb_dbg("SUNXI_USB_EFEX_EXIT\n");
#if defined(SUNXI_USB_30)
			if(sunxi_usb_efex_status_enable == 1)
			{
				sunxi_ubuf->rx_ready_for_data = 0;

				__sunxi_efex_send_status(&csw, sizeof(struct sunxi_efex_csw_t));
			}
			else if(sunxi_usb_efex_status_enable >= 2)
			{
				return sunxi_efex_next_action;
			}
#else
			sunxi_ubuf->rx_ready_for_data = 0;

			__sunxi_efex_send_status(&csw, sizeof(struct sunxi_efex_csw_t));

			return sunxi_efex_next_action;
#endif
	  	default:
	  		break;
	}

	return 0;
}


sunxi_usb_module_init(SUNXI_USB_DEVICE_EFEX,					\
					  sunxi_efex_init,							\
					  sunxi_efex_exit,							\
					  sunxi_efex_reset,							\
					  sunxi_efex_standard_req_op,				\
					  sunxi_efex_nonstandard_req_op,			\
					  sunxi_efex_state_loop,					\
					  sunxi_efex_usb_rx_dma_isr,				\
					  sunxi_efex_usb_tx_dma_isr					\
					  );