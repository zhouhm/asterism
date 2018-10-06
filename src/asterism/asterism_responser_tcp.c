#include "asterism_responser_tcp.h"
#include "asterism_core.h"
#include "asterism_utils.h"
#include "asterism_log.h"

static void responser_close_cb(
	uv_handle_t *handle)
{
	struct asterism_tcp_responser_s *responser = (struct asterism_tcp_responser_s *)handle;
	asterism_safefree(responser->host_rhs);
	AS_FREE(responser);
	asterism_log(ASTERISM_LOG_DEBUG, "responser is closing");
}

static void handshake_write_cb(
	uv_write_t *req,
	int status)
{
	struct asterism_write_req_s *write_req = (struct asterism_write_req_s *)req;
	free(write_req->write_buffer.base);
	free(write_req);
}

static int responser_connect_ack(
	struct asterism_tcp_responser_s *responser) {
	int ret = 0;

	struct asterism_trans_proto_s *connect_data =
		(struct asterism_trans_proto_s *)malloc(sizeof(struct asterism_trans_proto_s) + 4);

	connect_data->version = ASTERISM_TRANS_PROTO_VERSION;
	connect_data->cmd = ASTERISM_TRANS_PROTO_CONNECT_ACK;

	char *off = (char *)connect_data + sizeof(struct asterism_trans_proto_s);
	*(uint32_t *)off = htonl(responser->handshake_id);
	off += 4;
	uint16_t packet_len = (uint16_t)(off - (char *)connect_data);
	connect_data->len = htons((uint16_t)(packet_len));

	struct asterism_write_req_s* req = __zero_malloc_st(struct asterism_write_req_s);
	req->write_buffer.base = (char *)connect_data;
	req->write_buffer.len = packet_len;

	int write_ret = uv_write((uv_write_t*)req, (uv_stream_t*)responser, &req->write_buffer, 1, handshake_write_cb);
	if (write_ret != 0) {
		free(req->write_buffer.base);
		free(req);
		return -1;
	}

	asterism_log(ASTERISM_LOG_DEBUG, "responser send connect ack");
	return ret;
}

static void responser_connect_cb(
	uv_connect_t *req,
	int status)
{
	int ret = 0;
	struct asterism_tcp_responser_s *responser = (struct asterism_tcp_responser_s *)req->data;
	ret = responser_connect_ack(responser);
	if (ret != 0)
	{
		goto cleanup;
	}
	responser->link->link = (struct asterism_stream_s *)responser;
	ret = asterism_stream_read(responser->link);
	if (ret != 0)
	{
		goto cleanup;
	}
	ret = asterism_stream_read((struct asterism_stream_s*)responser);
	if (ret != 0)
	{
		goto cleanup;
	}
cleanup:
	if (ret != 0)
	{
		asterism_stream_close((struct asterism_stream_s*)responser);
	}
}

int asterism_responser_tcp_init(
	struct asterism_s *as,
	const char *host, unsigned int port,
	unsigned int handshake_id,
	struct asterism_stream_s* stream)
{
	int ret = 0;
	struct asterism_tcp_responser_s *responser = __zero_malloc_st(struct asterism_tcp_responser_s);
	ret = asterism_stream_connect(as, host, port,
		responser_connect_cb, 0, 0, responser_close_cb, (struct asterism_stream_s*)responser);
	if (ret)
		goto cleanup;
	asterism_stream_set_trans_mode((struct asterism_stream_s*)responser);
	responser->handshake_id = handshake_id;
	responser->link = stream;
cleanup:
	if (ret)
	{
		asterism_stream_close((struct asterism_stream_s*)responser);
	}
	return ret;
}
