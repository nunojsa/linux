/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _IIO_BACKEND_H_
#define _IIO_BACKEND_H_

#include <linux/types.h>

struct fwnode_handle;
struct iio_backend;
struct device;
struct iio_dev;
struct iio_chan_spec;

enum iio_backend_data_type {
	IIO_BACKEND_TWOS_COMPLEMENT,
	IIO_BACKEND_OFFSET_BINARY,
	IIO_BACKEND_DATA_TYPE_MAX
};

enum iio_backend_data_source {
	IIO_BACKEND_INTERNAL_CW,
	IIO_BACKEND_EXTERNAL,
	IIO_BACKEND_DATA_SOURCE_MAX
};

/**
 * struct iio_backend_data_fmt - Backend data format
 * @type:		Data type.
 * @sign_extend:	Bool to tell if the data is sign extended.
 * @enable:		Enable/Disable the data format module. If disabled,
 *			not formatting will happen.
 */
struct iio_backend_data_fmt {
	enum iio_backend_data_type type;
	bool sign_extend;
	bool enable;
};

/**
 * struct iio_backend_ops - operations structure for an iio_backend
 * @enable:		Enable backend.
 * @disable:		Disable backend.
 * @chan_enable:	Enable one channel.
 * @chan_disable:	Disable one channel.
 * @data_format_set:	Configure the data format for a specific channel.
 * @request_buffer:	Request an IIO buffer.
 * @free_buffer:	Free an IIO buffer.
 **/
struct iio_backend_ops {
	int (*enable)(struct iio_backend *back);
	void (*disable)(struct iio_backend *back);
	int (*chan_enable)(struct iio_backend *back, unsigned int chan);
	int (*chan_disable)(struct iio_backend *back, unsigned int chan);
	int (*data_format_set)(struct iio_backend *back, unsigned int chan,
			       const struct iio_backend_data_fmt *data);
	int (*data_source_set)(struct iio_backend *back, unsigned int chan,
			       enum iio_backend_data_source data);
	struct iio_buffer *(*request_buffer)(struct iio_backend *back,
					     struct iio_dev *indio_dev);
	void (*free_buffer)(struct iio_backend *back,
			    struct iio_buffer *buffer);
	int (*read_raw)(struct iio_backend *back,
			struct iio_chan_spec const *chan, int *val, int *val2,
			long mask);
	int (*write_raw)(struct iio_backend *back,
			 struct iio_chan_spec const *chan, int val, int val2,
			 long mask);
};

int iio_backend_chan_enable(struct iio_backend *back, unsigned int chan);
int iio_backend_chan_disable(struct iio_backend *back, unsigned int chan);
int devm_iio_backend_enable(struct device *dev, struct iio_backend *back);
int iio_backend_data_format_set(struct iio_backend *back, unsigned int chan,
				const struct iio_backend_data_fmt *data);
int iio_backend_data_source_set(struct iio_backend *back, unsigned int chan,
				enum iio_backend_data_source data);
int devm_iio_backend_request_buffer(struct device *dev,
				    struct iio_backend *back,
				    struct iio_dev *indio_dev);
int iio_backend_read_raw(struct iio_backend *back,
			 struct iio_chan_spec const *chan, int *val, int *val2,
			 long mask);
int iio_backend_write_raw(struct iio_backend *back,
			  struct iio_chan_spec const *chan, int val, int val2,
			  long mask);
void *iio_backend_get_priv(const struct iio_backend *conv);
struct iio_backend *devm_iio_backend_get(struct device *dev, const char *name);
struct iio_backend *
__devm_iio_backend_get_from_fwnode_lookup(struct device *dev,
					  struct fwnode_handle *fwnode);

int devm_iio_backend_register(struct device *dev,
			      const struct iio_backend_ops *ops, void *priv);

#endif
