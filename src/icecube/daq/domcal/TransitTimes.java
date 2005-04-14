package icecube.daq.domcal;

import java.nio.ByteBuffer;

/**
 * Created by IntelliJ IDEA.
 * User: jbraun
 * Date: Apr 14, 2005
 * Time: 4:22:00 PM
 * To change this template use File | Settings | File Templates.
 */
public class TransitTimes {

    private short voltage;
    private float value;
    private float error;

    public static TransitTimes parseTransitTimes(ByteBuffer bb) {
        short v = bb.getShort();
        float val = bb.getFloat();
        float err = bb.getFloat();
        return new TransitTimes(v, val, err);
    }

    public TransitTimes(short voltage, float value, float error) {
        this.voltage = voltage;
        this.value = value;
        this.error = error;
    }

    public short getVoltage() {
        return voltage;
    }

    public float getValue() {
        return value;
    }

    public float getError() {
        return error;
    }
}
