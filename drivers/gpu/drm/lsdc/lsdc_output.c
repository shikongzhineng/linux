// SPDX-License-Identifier: GPL-2.0

#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_probe_helper.h>
#include "lsdc_drv.h"

static int lsdc_get_modes(struct drm_connector *connector)
{
	unsigned int num = 0;
	struct edid *edid;

	if (connector->ddc) {
		edid = drm_get_edid(connector, connector->ddc);
		if (edid) {
			drm_connector_update_edid_property(connector, edid);
			num = drm_add_edid_modes(connector, edid);
			kfree(edid);
		}

		return num;
	}

	num = drm_add_modes_noedid(connector, 1920, 1200);

	drm_set_preferred_mode(connector, 1024, 768);

	return num;
}

static enum drm_connector_status
lsdc_unknown_connector_detect(struct drm_connector *connector, bool force)
{
	struct i2c_adapter *ddc = connector->ddc;

	if (ddc) {
		if (drm_probe_ddc(ddc))
			return connector_status_connected;
	} else {
		if (connector->connector_type == DRM_MODE_CONNECTOR_DPI)
			return connector_status_connected;

		if (connector->connector_type == DRM_MODE_CONNECTOR_VIRTUAL)
			return connector_status_connected;
	}

	return connector_status_unknown;
}

static enum drm_connector_status
lsdc_hdmi_connector_detect(struct drm_connector *connector, bool force)
{
	struct lsdc_display_pipe *pipe = connector_to_display_pipe(connector);
	struct lsdc_device *ldev = to_lsdc(connector->dev);
	u32 val;

	val = lsdc_rreg32(ldev, LSDC_HDMI_HPD_STATUS_REG);

	if (pipe->index == 0) {
		if (val & HDMI0_HPD_FLAG)
			return connector_status_connected;
	}

	if (pipe->index == 1) {
		if (val & HDMI1_HPD_FLAG)
			return connector_status_connected;
	}

	return connector_status_disconnected;
}

static enum drm_connector_status
lsdc_hdmi_vga_connector_detect(struct drm_connector *connector, bool force)
{
	struct lsdc_display_pipe *pipe = connector_to_display_pipe(connector);
	struct drm_device *ddev = connector->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	struct i2c_adapter *ddc;
	u32 val;

	val = lsdc_rreg32(ldev, LSDC_HDMI_HPD_STATUS_REG);

	if (pipe->index == 1) {
		if (val & HDMI1_HPD_FLAG)
			return connector_status_connected;

		return connector_status_disconnected;
	}

	if (pipe->index == 0) {
		if (val & HDMI0_HPD_FLAG)
			return connector_status_connected;

		ddc = connector->ddc;
		if (ddc) {
			if (drm_probe_ddc(ddc))
				return connector_status_connected;

			return connector_status_disconnected;
		}
	}

	return connector_status_unknown;
}

static struct drm_encoder *
lsdc_connector_get_best_encoder(struct drm_connector *connector,
				struct drm_atomic_state *state)
{
	struct lsdc_display_pipe *pipe = connector_to_display_pipe(connector);

	return &pipe->encoder;
}

static const struct drm_connector_helper_funcs lsdc_connector_helpers = {
	.atomic_best_encoder = lsdc_connector_get_best_encoder,
	.get_modes = lsdc_get_modes,
};

static const struct drm_connector_funcs lsdc_unknown_connector_funcs = {
	.detect = lsdc_unknown_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_funcs lsdc_hdmi_vga_connector_funcs = {
	.detect = lsdc_hdmi_vga_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_funcs lsdc_hdmi_connector_funcs = {
	.detect = lsdc_hdmi_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_funcs *
lsdc_get_connector_func(struct lsdc_device *ldev, unsigned int index)
{
	const struct lsdc_desc *descp = ldev->descp;

	if (descp->chip == CHIP_LS7A2000) {
		if (index == 0)
			return &lsdc_hdmi_vga_connector_funcs;

		if (index == 1)
			return &lsdc_hdmi_connector_funcs;
	}

	return &lsdc_unknown_connector_funcs;
}

static int lsdc_get_encoder_type(struct lsdc_device *ldev,
				 unsigned int index)
{
	const struct lsdc_desc *descp = ldev->descp;

	if (descp->chip == CHIP_LS7A2000) {
		if (index == 0)
			return DRM_MODE_ENCODER_DAC;
		if (index == 1)
			return DRM_MODE_ENCODER_TMDS;
	}

	if (descp->chip == CHIP_LS7A1000 || descp->chip == CHIP_LS2K1000) {
		if (index == 0)
			return DRM_MODE_ENCODER_DPI;
		if (index == 1)
			return DRM_MODE_ENCODER_DPI;
	}

	return DRM_MODE_ENCODER_NONE;
}

static int lsdc_get_connector_type(struct lsdc_device *ldev,
				   unsigned int index)
{
	const struct lsdc_desc *descp = ldev->descp;

	if (descp->chip == CHIP_LS7A2000) {
		if (index == 0)
			return DRM_MODE_CONNECTOR_VGA;
		if (index == 1)
			return DRM_MODE_CONNECTOR_HDMIA;
	}

	if (descp->chip == CHIP_LS7A1000 || descp->chip == CHIP_LS2K1000) {
		if (index == 0)
			return DRM_MODE_CONNECTOR_DPI;
		if (index == 1)
			return DRM_MODE_CONNECTOR_DPI;
	}

	return DRM_MODE_CONNECTOR_Unknown;
}

static int lsdc_hdmi_set_avi_infoframe(struct drm_encoder *encoder,
				       struct drm_display_mode *mode)
{
	struct drm_device *ddev = encoder->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	struct lsdc_display_pipe *dispipe = encoder_to_display_pipe(encoder);
	unsigned int index = dispipe->index;
	struct hdmi_avi_infoframe infoframe;
	u8 buffer[HDMI_INFOFRAME_SIZE(AVI)];
	unsigned char *ptr = &buffer[HDMI_INFOFRAME_HEADER_SIZE];
	unsigned int content0, content1, content2, content3;
	int err;

	err = drm_hdmi_avi_infoframe_from_display_mode(&infoframe, &dispipe->connector, mode);
	if (err < 0) {
		drm_err(ddev, "failed to setup AVI infoframe: %d\n", err);
		return err;
	}

	/* Fixed infoframe configuration not linked to the mode */
	infoframe.colorspace = HDMI_COLORSPACE_RGB;
	infoframe.quantization_range = HDMI_QUANTIZATION_RANGE_DEFAULT;
	infoframe.colorimetry = HDMI_COLORIMETRY_NONE;

	err = hdmi_avi_infoframe_pack(&infoframe, buffer, sizeof(buffer));
	if (err < 0) {
		drm_err(ddev, "failed to pack AVI infoframe: %d\n", err);
			return err;
	}

	content0 = *(unsigned int *)ptr;
	content1 = *(ptr + 4);
	content2 = *(unsigned int *)(ptr + 5);
	content3 = *(unsigned int *)(ptr + 9);

	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_AVI_CONTENT0, index, content0);
	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_AVI_CONTENT1, index, content1);
	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_AVI_CONTENT2, index, content2);
	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_AVI_CONTENT3, index, content3);

	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_AVI_INFO_CRTL, index,
			 AVI_PKT_ENABLE | AVI_PKT_SEND_FREQ | AVI_PKT_UPDATE);

	drm_dbg(ddev, "Update HDMI-%u avi infoframe\n", index);

	return 0;
}

static void lsdc_hdmi_atomic_disable(struct drm_encoder *encoder,
				     struct drm_atomic_state *state)
{
	struct lsdc_display_pipe *dispipe = encoder_to_display_pipe(encoder);
	struct drm_device *ddev = encoder->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	unsigned int index = dispipe->index;
	u32 val;

	val = lsdc_hdmi_rreg32(ldev, LSDC_HDMI0_PHY_CTRL_REG, index);
	val &= ~HDMI_PHY_EN;
	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_PHY_CTRL_REG, index, val);

	drm_dbg(ddev, "HDMI-%u disabled\n", index);
}

static void lsdc_hdmi_atomic_enable(struct drm_encoder *encoder,
				    struct drm_atomic_state *state)
{
	struct drm_device *ddev = encoder->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	struct lsdc_display_pipe *dispipe = encoder_to_display_pipe(encoder);
	unsigned int index = dispipe->index;
	u32 video_preamble_length = 8;
	u32 val;

	/* we are using software gpio emulated i2c */
	val = HDMI_CTL_PERIOD_MODE |
	      HDMI_AUDIO_EN |
	      HDMI_PACKET_EN |
	      HDMI_INTERFACE_EN;

	val |= video_preamble_length < HDMI_VIDEO_PREAMBLE_SHIFT;

	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_CTRL_REG, index, val);

	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_ZONE_REG, index, 0x00400040);

	val = HDMI_PHY_TERM_STATUS |
	      HDMI_PHY_TERM_DET_EN |
	      HDMI_PHY_TERM_H_EN |
	      HDMI_PHY_TERM_L_EN |
	      HDMI_PHY_RESET_N |
	      HDMI_PHY_EN;

	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_PHY_CTRL_REG, index, val);

	drm_dbg(ddev, "HDMI-%u enabled\n", index);
}

/*
 *  Fout = M * Fin
 *
 *  M = (4 * LF) / (IDF * ODF)
 *
 *  Loongson HDMI require M = 10
 */
static void lsdc_hdmi_phy_pll_config(struct lsdc_device *ldev,
				     unsigned int index,
				     int clock)
{
	struct drm_device *ddev = &ldev->base;
	int count = 0;
	u32 val;

	/* disable phy pll */
	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_PHY_PLL_REG, index, 0x0);

	/*
	 * 10 = (4 * 40) / (8 * 2)
	 */
	val = (8 << HDMI_PLL_IDF_SHIFT) |
	      (40 << HDMI_PLL_LF_SHIFT) |
	      (1 << HDMI_PLL_ODF_SHIFT) |
	      HDMI_PLL_ENABLE;

	drm_dbg(ddev, "HDMI-%u clock: %d\n", index, clock);

	lsdc_hdmi_wreg32(ldev, LSDC_HDMI0_PHY_PLL_REG, index, val);

	/* wait hdmi phy pll lock */
	do {
		val = lsdc_hdmi_rreg32(ldev, LSDC_HDMI0_PHY_PLL_REG, index);

		++count;

		if (val & HDMI_PLL_LOCKED) {
			drm_dbg(ddev, "Setting HDMI-%u PLL success(take %d cycles)\n",
				index, count);
			break;
		}
	} while (count < 1000);

	if (count >= 1000)
		drm_err(ddev, "Setting HDMI-%u PLL failed\n", index);
}

static void lsdc_hdmi_atomic_mode_set(struct drm_encoder *encoder,
				      struct drm_crtc_state *crtc_state,
				      struct drm_connector_state *conn_state)
{
	struct drm_device *ddev = encoder->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	struct lsdc_display_pipe *dispipe = encoder_to_display_pipe(encoder);
	unsigned int index = dispipe->index;
	struct drm_display_mode *mode = &crtc_state->mode;

	lsdc_hdmi_phy_pll_config(ldev, index, mode->clock);

	lsdc_hdmi_set_avi_infoframe(encoder, mode);

	drm_dbg(ddev, "HDMI-%u modeset\n", index);
}

static const struct drm_encoder_helper_funcs lsdc_hdmi_helper_funcs = {
	.atomic_disable = lsdc_hdmi_atomic_disable,
	.atomic_enable = lsdc_hdmi_atomic_enable,
	.atomic_mode_set = lsdc_hdmi_atomic_mode_set,
};

static void lsdc_hdmi_reset(struct drm_encoder *encoder)
{
	struct drm_device *ddev = encoder->dev;
	struct lsdc_device *ldev = to_lsdc(ddev);
	struct lsdc_display_pipe *dispipe = encoder_to_display_pipe(encoder);
	unsigned int index = dispipe->index;
	u32 val = HDMI_CTL_PERIOD_MODE |
		  HDMI_AUDIO_EN |
		  HDMI_PACKET_EN |
		  HDMI_INTERFACE_EN;

	lsdc_wreg32(ldev, LSDC_HDMI0_CTRL_REG, val);
	lsdc_wreg32(ldev, LSDC_HDMI1_CTRL_REG, val);

	drm_dbg(ddev, "HDMI-%u Reset\n", index);
}

static const struct drm_encoder_funcs lsdc_encoder_funcs = {
	.reset = lsdc_hdmi_reset,
	.destroy = drm_encoder_cleanup,
};

int lsdc_create_output(struct lsdc_device *ldev, unsigned int index)
{
	const struct lsdc_desc *descp = ldev->descp;
	struct drm_device *ddev = &ldev->base;
	struct lsdc_display_pipe *dispipe = &ldev->dispipe[index];
	struct drm_encoder *encoder = &dispipe->encoder;
	struct drm_connector *connector = &dispipe->connector;
	struct i2c_adapter *ddc = NULL;
	struct lsdc_i2c *li2c;
	int ret;

	ret = drm_encoder_init(ddev,
			       encoder,
			       &lsdc_encoder_funcs,
			       lsdc_get_encoder_type(ldev, index),
			       "encoder-%u",
			       index);
	if (ret) {
		drm_err(ddev, "Failed to init encoder: %d\n", ret);
		return ret;
	}

	encoder->possible_crtcs = BIT(index);

	if (descp->chip == CHIP_LS7A2000)
		drm_encoder_helper_add(encoder, &lsdc_hdmi_helper_funcs);

	if (descp->has_builtin_i2c) {
		li2c = lsdc_create_i2c_chan(ddev, ldev->reg_base, index);
		if (IS_ERR(li2c))
			return PTR_ERR(li2c);

		dispipe->li2c = li2c;

		ddc = &li2c->adapter;
	} else {
		ddc = lsdc_get_i2c_adapter(ldev, index);
		if (IS_ERR(ddc)) {
			drm_err(ddev, "Error get ddc for output-%u\n", index);
			return PTR_ERR(ddc);
		}
	}

	ret = drm_connector_init_with_ddc(ddev,
					  connector,
					  lsdc_get_connector_func(ldev, index),
					  lsdc_get_connector_type(ldev, index),
					  ddc);
	if (ret) {
		drm_err(ddev, "Init connector-%d failed\n", index);
		return ret;
	}

	drm_connector_helper_add(connector, &lsdc_connector_helpers);

	drm_connector_attach_encoder(connector, encoder);

	connector->polled = DRM_CONNECTOR_POLL_CONNECT |
			    DRM_CONNECTOR_POLL_DISCONNECT;

	connector->interlace_allowed = 0;
	connector->doublescan_allowed = 0;

	ldev->num_output++;

	drm_info(ddev, "output-%u initialized\n", index);

	return 0;
}
