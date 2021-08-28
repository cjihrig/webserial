#include <string.h>
#include <node_api.h>
#include <libserialport.h>
#include "serial-handle.h"

#define NAPI_CHECK(status, msg)                                               \
  do {                                                                        \
    if ((status) != napi_ok) {                                                \
      const napi_extended_error_info* error_info = NULL;                      \
      napi_get_last_error_info((env), &error_info);                           \
      bool is_pending;                                                        \
      napi_is_exception_pending((env), &is_pending);                          \
      if (!is_pending) {                                                      \
        const char* message = (error_info->error_message == NULL)             \
            ? (msg)                                                           \
            : error_info->error_message;                                      \
        napi_throw_error((env), NULL, message);                               \
        return NULL;                                                          \
      }                                                                       \
    }                                                                         \
  } while(0)

#define SP_CHECK(result)                                                      \
  do {                                                                        \
    if ((result) != SP_OK) {                                                  \
      char const* message;                                                    \
                                                                              \
      switch ((result)) {                                                     \
        case SP_ERR_ARG:                                                      \
          message = "Invalid argument";                                       \
          break;                                                              \
        case SP_ERR_FAIL:                                                     \
          message = sp_last_error_message();                                  \
          break;                                                              \
        case SP_ERR_SUPP:                                                     \
          message = "Not supported";                                          \
          break;                                                              \
        case SP_ERR_MEM:                                                      \
          message = "Out of memory";                                          \
          break;                                                              \
        default:                                                              \
          message = "Unknown serial port error";                              \
          break;                                                              \
      }                                                                       \
                                                                              \
      napi_throw_error((env), NULL, message);                                 \
                                                                              \
      if ((result) == SP_ERR_FAIL) {                                          \
        sp_free_error_message((char*)message);                                \
      }                                                                       \
                                                                              \
      return NULL;                                                            \
    }                                                                         \
  } while(0)

#define EXPORT_FUNCTION_OR_RETURN(env, exports, func, name)                   \
  do {                                                                        \
    napi_status status;                                                       \
    napi_value fn;                                                            \
                                                                              \
    status = napi_create_function((env), nullptr, 0, (func), nullptr, &fn);   \
    if (status != napi_ok) {                                                  \
      return nullptr;                                                         \
    }                                                                         \
                                                                              \
    status = napi_set_named_property((env), (exports), (name), fn);           \
    if (status != napi_ok) {                                                  \
      return nullptr;                                                         \
    }                                                                         \
  } while(0)

#define EXPORT_INT_OR_RETURN(env, exports, value, name)                       \
  do {                                                                        \
    napi_status status;                                                       \
    napi_value val;                                                           \
                                                                              \
    status = napi_create_int32((env), (value), &val);                         \
    if (status != napi_ok) {                                                  \
      return nullptr;                                                         \
    }                                                                         \
                                                                              \
    status = napi_set_named_property((env), (exports), (name), val);          \
    if (status != napi_ok) {                                                  \
      return nullptr;                                                         \
    }                                                                         \
  } while(0)

namespace webserial {

napi_value CreateHandle(napi_env env, napi_callback_info args) {
  struct sp_port* port;
  napi_value ret;
  napi_value argv[1];
  size_t len;
  size_t argc = 1;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );

  NAPI_CHECK(
    napi_get_value_string_utf8(env, argv[0], nullptr, 0, &len),
    "could not get port name length"
  );

  char port_name[len + 1];

  NAPI_CHECK(
    napi_get_value_string_utf8(
      env,
      argv[0],
      port_name,
      sizeof(port_name),
      &len
    ),
    "could not get port name length"
  );

  SP_CHECK(sp_get_port_by_name(port_name, &port));
  SerialHandle::NewInstance(env, port, &ret);

  return ret;
}

napi_value ClosePort(napi_env env, napi_callback_info args) {
  SerialHandle* handle;
  napi_value argv[1];
  napi_value ret;
  size_t argc = 1;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );
  NAPI_CHECK(
    napi_unwrap(env, argv[0], reinterpret_cast<void**>(&handle)),
    "could not unwrap handle"
  );
  SP_CHECK(handle->close_port());
  NAPI_CHECK(napi_get_undefined(env, &ret), "could not get undefined");

  return ret;
}

napi_value OpenPort(napi_env env, napi_callback_info args) {
  SerialHandle* handle;
  napi_value argv[6];
  napi_value ret;
  size_t argc = 6;
  int baud_rate;
  int data_bits;
  int stop_bits;
  int parity;
  int flow_control;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );
  NAPI_CHECK(
    napi_unwrap(env, argv[0], reinterpret_cast<void**>(&handle)),
    "could not unwrap handle"
  );
  NAPI_CHECK(
    napi_get_value_int32(env, argv[1], &baud_rate),
    "could not get baudRate"
  );
  NAPI_CHECK(
    napi_get_value_int32(env, argv[2], &data_bits),
    "could not get dataBits"
  );
  NAPI_CHECK(
    napi_get_value_int32(env, argv[3], &stop_bits),
    "could not get stopBits"
  );
  NAPI_CHECK(
    napi_get_value_int32(env, argv[4], &parity),
    "could not get parity"
  );
  NAPI_CHECK(
    napi_get_value_int32(env, argv[5], &flow_control),
    "could not get flowControl"
  );
  SP_CHECK(handle->open_port(baud_rate,
                             data_bits,
                             stop_bits,
                             parity,
                             flow_control));
  NAPI_CHECK(napi_get_undefined(env, &ret), "could not get undefined");

  return ret;
}

napi_value GetSignals(napi_env env, napi_callback_info args) {
  SerialHandle* handle;
  napi_value argv[1];
  napi_value field;
  napi_value ret;
  size_t argc = 1;
  int cts;
  int dsr;
  int dcd;
  int ri;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );
  NAPI_CHECK(
    napi_unwrap(env, argv[0], reinterpret_cast<void**>(&handle)),
    "could not unwrap handle"
  );
  SP_CHECK(handle->get_signals(&cts, &dsr, &dcd, &ri));
  NAPI_CHECK(napi_create_object(env, &ret), "could not create object");
  NAPI_CHECK(
    napi_get_boolean(env, cts != 0, &field),
    "could not create cts"
  );
  NAPI_CHECK(
    napi_set_named_property(env, ret, "clearToSend", field),
    "could not set 'clearToSend' property"
  );
  NAPI_CHECK(
    napi_get_boolean(env, dcd != 0, &field),
    "could not create dcd"
  );
  NAPI_CHECK(
    napi_set_named_property(env, ret, "dataCarrierDetect", field),
    "could not set 'dataCarrierDetect' property"
  );
  NAPI_CHECK(
    napi_get_boolean(env, dsr != 0, &field),
    "could not create dsr"
  );
  NAPI_CHECK(
    napi_set_named_property(env, ret, "dataSetReady", field),
    "could not set 'dataSetReady' property"
  );
  NAPI_CHECK(
    napi_get_boolean(env, ri != 0, &field),
    "could not create ri"
  );
  NAPI_CHECK(
    napi_set_named_property(env, ret, "ringIndicator", field),
    "could not set 'ringIndicator' property"
  );

  return ret;
}

napi_value SetSignals(napi_env env, napi_callback_info args) {
  SerialHandle* handle;
  napi_value argv[4];
  napi_value ret;
  size_t argc = 4;
  int dtr;
  int rts;
  int brk;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );
  NAPI_CHECK(
    napi_unwrap(env, argv[0], reinterpret_cast<void**>(&handle)),
    "could not unwrap handle"
  );

  NAPI_CHECK(napi_get_value_int32(env, argv[1], &dtr), "could not get dtr");
  NAPI_CHECK(napi_get_value_int32(env, argv[2], &rts), "could not get rts");
  NAPI_CHECK(napi_get_value_int32(env, argv[3], &brk), "could not get brk");
  SP_CHECK(handle->set_signals(dtr, rts, brk));
  NAPI_CHECK(napi_get_undefined(env, &ret), "could not get undefined");

  return ret;
}

napi_value ReadData(napi_env env, napi_callback_info args) {
  SerialHandle* handle;
  napi_value argv[2];
  napi_value field;
  napi_value ret = nullptr;
  size_t argc = 2;
  uint32_t bytes_to_read;
  ssize_t bytes_read;
  void* backing_store;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );
  NAPI_CHECK(
    napi_unwrap(env, argv[0], reinterpret_cast<void**>(&handle)),
    "could not unwrap handle"
  );
  NAPI_CHECK(
    napi_get_value_uint32(env, argv[1], &bytes_to_read),
    "could not get desired size"
  );
  NAPI_CHECK(
    napi_create_arraybuffer(env, bytes_to_read, &backing_store, &ret),
    "could not create array buffer"
  );

  bytes_read = handle->read_data(backing_store, bytes_to_read);
  if (bytes_read < 0) {
    SP_CHECK(bytes_read);
  }

  NAPI_CHECK(
    napi_create_int32(env, bytes_read, &field),
    "could not create bytesRead"
  );
  NAPI_CHECK(
    napi_set_named_property(env, ret, "bytesRead", field),
    "could not set 'bytesRead' property"
  );

  return ret;
}

napi_value WriteData(napi_env env, napi_callback_info args) {
  SerialHandle* handle;
  napi_value argv[2];
  napi_value ret = nullptr;
  size_t argc = 2;
  size_t bytes_to_write;
  ssize_t bytes_written;
  void* buf;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );
  NAPI_CHECK(
    napi_unwrap(env, argv[0], reinterpret_cast<void**>(&handle)),
    "could not unwrap handle"
  );
  NAPI_CHECK(
    napi_get_arraybuffer_info(env, argv[1], &buf, &bytes_to_write),
    "could not get buffer"
  );

  bytes_written = handle->write_data(buf, bytes_to_write);
  if (bytes_written < 0) {
    SP_CHECK(bytes_written);
  }

  return ret;
}

napi_value DiscardRxBuffer(napi_env env, napi_callback_info args) {
  SerialHandle* handle;
  napi_value argv[1];
  napi_value ret;
  size_t argc = 1;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );
  NAPI_CHECK(
    napi_unwrap(env, argv[0], reinterpret_cast<void**>(&handle)),
    "could not unwrap handle"
  );
  SP_CHECK(handle->discard_rx_buffer());
  NAPI_CHECK(napi_get_undefined(env, &ret), "could not get undefined");

  return ret;
}

napi_value FlushTxBuffer(napi_env env, napi_callback_info args) {
  SerialHandle* handle;
  napi_value argv[1];
  napi_value ret;
  size_t argc = 1;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );
  NAPI_CHECK(
    napi_unwrap(env, argv[0], reinterpret_cast<void**>(&handle)),
    "could not unwrap handle"
  );
  SP_CHECK(handle->flush_tx_buffer());
  NAPI_CHECK(napi_get_undefined(env, &ret), "could not get undefined");

  return ret;
}

napi_value DiscardTxBuffer(napi_env env, napi_callback_info args) {
  SerialHandle* handle;
  napi_value argv[1];
  napi_value ret;
  size_t argc = 1;

  NAPI_CHECK(
    napi_get_cb_info(env, args, &argc, argv, nullptr, nullptr),
    "could not get arguments"
  );
  NAPI_CHECK(
    napi_unwrap(env, argv[0], reinterpret_cast<void**>(&handle)),
    "could not unwrap handle"
  );
  SP_CHECK(handle->discard_tx_buffer());
  NAPI_CHECK(napi_get_undefined(env, &ret), "could not get undefined");

  return ret;
}

napi_value ListAllPorts(napi_env env, napi_callback_info args) {
  struct sp_port** port_list;
  napi_value ret;
  sp_return r;

  NAPI_CHECK(napi_create_array(env, &ret), "could not create array");
  SP_CHECK(sp_list_ports(&port_list));

  for (uint32_t i = 0; port_list[i] != NULL; ++i) {
    struct sp_port* port = port_list[i];
    char* port_name = sp_get_port_name(port);
    int vendor_id;
    int product_id;

    if (port_name == NULL) {
      continue;
    }

    size_t name_len = strlen(port_name);
    napi_status status;
    napi_value object;
    napi_value name;

    status = napi_create_object(env, &object);
    if (status != napi_ok) {
      sp_free_port_list(port_list);
      NAPI_CHECK(status, "could not create object");
    }

    status = napi_create_string_utf8(env, port_name, name_len, &name);
    if (status != napi_ok) {
      sp_free_port_list(port_list);
      NAPI_CHECK(status, "could not create string");
    }

    status = napi_set_named_property(env, object, "name", name);
    if (status != napi_ok) {
      sp_free_port_list(port_list);
      NAPI_CHECK(status, "could not set 'name' property");
    }

    status = napi_set_element(env, ret, i, object);
    if (status != napi_ok) {
      sp_free_port_list(port_list);
      NAPI_CHECK(status, "could not set array element");
    }

    r = sp_get_port_usb_vid_pid(port, &vendor_id, &product_id);
    if (r != SP_OK && r != SP_ERR_ARG) {
      sp_free_port_list(port_list);
      SP_CHECK(r);
    }

    if (r == SP_OK) {
      napi_value vendor;
      napi_value product;

      status = napi_create_int32(env, vendor_id, &vendor);
      if (status != napi_ok) {
        sp_free_port_list(port_list);
        NAPI_CHECK(status, "could not create vendor value");
      }

      status = napi_create_int32(env, product_id, &product);
      if (status != napi_ok) {
        sp_free_port_list(port_list);
        NAPI_CHECK(status, "could not create product value");
      }

      status = napi_set_named_property(env, object, "vendorId", vendor);
      if (status != napi_ok) {
        sp_free_port_list(port_list);
        NAPI_CHECK(status, "could not set 'vendorId' property");
      }

      status = napi_set_named_property(env, object, "productId", product);
      if (status != napi_ok) {
        sp_free_port_list(port_list);
        NAPI_CHECK(status, "could not set 'productId' property");
      }
    }
  }

  sp_free_port_list(port_list);

  return ret;
}

napi_value init(napi_env env, napi_value exports) {
  SerialHandle::Init(env);

  EXPORT_FUNCTION_OR_RETURN(env, exports, CreateHandle, "createHandle");
  EXPORT_FUNCTION_OR_RETURN(env, exports, ListAllPorts, "listAllPorts");
  EXPORT_FUNCTION_OR_RETURN(env, exports, OpenPort, "openPort");
  EXPORT_FUNCTION_OR_RETURN(env, exports, ClosePort, "closePort");
  EXPORT_FUNCTION_OR_RETURN(env, exports, GetSignals, "getSignals");
  EXPORT_FUNCTION_OR_RETURN(env, exports, SetSignals, "setSignals");
  EXPORT_FUNCTION_OR_RETURN(env, exports, ReadData, "readData");
  EXPORT_FUNCTION_OR_RETURN(env, exports, WriteData, "writeData");
  EXPORT_FUNCTION_OR_RETURN(env, exports, DiscardRxBuffer, "discardRxBuffer");
  EXPORT_FUNCTION_OR_RETURN(env, exports, FlushTxBuffer, "flushTxBuffer");
  EXPORT_FUNCTION_OR_RETURN(env, exports, DiscardTxBuffer, "discardTxBuffer");

  EXPORT_INT_OR_RETURN(env, exports, SP_PARITY_NONE, "kParityNone");
  EXPORT_INT_OR_RETURN(env, exports, SP_PARITY_ODD, "kParityOdd");
  EXPORT_INT_OR_RETURN(env, exports, SP_PARITY_EVEN, "kParityEven");

  EXPORT_INT_OR_RETURN(env, exports, SP_FLOWCONTROL_NONE, "kFlowControlNone");
  EXPORT_INT_OR_RETURN(
    env,
    exports,
    SP_FLOWCONTROL_RTSCTS,
    "kFlowControlHardware"
  );

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)

}
