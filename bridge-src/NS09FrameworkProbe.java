import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;

/** Read-only framework inspection; never invokes vibrate/cancel or changes settings. */
public final class NS09FrameworkProbe {
    private static String targetAddress="";
    private static Object call(Class<?> owner, Object object, String method, Class<?>[] types, Object... args) throws Exception {
        return owner.getMethod(method, types).invoke(object, args);
    }
    private static Object get(Class<?> owner, Object object, String method) throws Exception {
        return call(owner, object, method, new Class<?>[0]);
    }
    private static int snapshot() throws Exception {
        Class<?> device = Class.forName("android.view.InputDevice");
        Class<?> manager = Class.forName("android.os.VibratorManager");
        Class<?> vibrator = Class.forName("android.os.Vibrator");
        int[] ids = (int[])get(device, null, "getDeviceIds");
        int matchingPads = 0, bridgePads = 0, physicalPads = 0;
        boolean dual = false, xboxAxes = false, xboxButtons = false;
        for (int id : ids) {
            Object d = call(device, null, "getDevice", new Class<?>[]{int.class}, id);
            if (d == null) continue;
            String name = (String)get(device, d, "getName");
            int vid = (Integer)get(device, d, "getVendorId");
            int pid = (Integer)get(device, d, "getProductId");
            int sources = (Integer)get(device, d, "getSources");
            String bluetooth = "unavailable";
            try { bluetooth = String.valueOf(get(device,d,"getBluetoothAddress")); } catch (Throwable ignored) { }
            boolean physicalNS09 = !targetAddress.isEmpty() && bluetooth.replace(":", "").equalsIgnoreCase(targetAddress);
            boolean enabled = (Boolean)get(device,d,"isEnabled");
            boolean joystick = (sources & 0x01000010) == 0x01000010;
            if (joystick && vid==0x045e && pid==0x02e0 && (physicalNS09 || !bluetooth.matches("(?i)[0-9a-f]{2}(:[0-9a-f]{2}){5}"))) physicalPads++;
            Object vm = get(device, d, "getVibratorManager");
            int[] channels = (int[])get(manager, vm, "getVibratorIds");
            boolean allAmplitude = channels.length >= 2;
            StringBuilder caps = new StringBuilder();
            for (int channel : channels) {
                Object v = call(manager, vm, "getVibrator", new Class<?>[]{int.class}, channel);
                boolean present = (Boolean)get(vibrator, v, "hasVibrator");
                boolean amplitude = (Boolean)get(vibrator, v, "hasAmplitudeControl");
                allAmplitude &= present && amplitude;
                caps.append(" id=").append(channel).append(" present=").append(present).append(" amplitude=").append(amplitude);
            }
            System.out.println("InputDevice id="+id+" name="+name+" vid="+vid+" pid="+pid+" sources=0x"+Integer.toHexString(sources)+
                " descriptor="+get(device,d,"getDescriptor")+" enabled="+enabled+" bluetooth="+bluetooth+
                " physicalNS09AddressMatch="+physicalNS09+" vibratorIds="+Arrays.toString(channels)+caps);
            if (joystick && vid == 0x045e && pid == 0x028e) {
                matchingPads++;
                if (name.equals("Microsoft X-Box 360 pad")) {
                    bridgePads++; dual = enabled && allAmplitude && channels[0] == 0 && channels[1] == 1;
                    int[] keyCodes={96,97,99,100,102,103,106,107,108,109};
                    boolean[] keyPresent=(boolean[])call(device,d,"hasKeys",new Class<?>[]{int[].class},keyCodes);
                    xboxButtons=true; for(boolean present:keyPresent) xboxButtons &= present;
                    System.out.println("XBOX_BUTTONS androidKeyCodes="+Arrays.toString(keyCodes)+" present="+Arrays.toString(keyPresent));
                    Class<?> motionRange=Class.forName("android.view.InputDevice$MotionRange");
                    List<?> ranges=(List<?>)get(device,d,"getMotionRanges");
                    boolean[] found=new boolean[8]; int[] axes={0,1,11,14,17,18,15,16};
                    for(Object r:ranges) {
                        int axis=(Integer)get(motionRange,r,"getAxis");
                        float min=(Float)get(motionRange,r,"getMin"), max=(Float)get(motionRange,r,"getMax");
                        int source=(Integer)get(motionRange,r,"getSource");
                        System.out.println("XBOX_AXIS androidAxis="+axis+" min="+min+" max="+max+" flat="+get(motionRange,r,"getFlat")+" source=0x"+Integer.toHexString(source));
                        for(int i=0;i<axes.length;i++) if(axis==axes[i] && (source & 0x01000010)==0x01000010)
                            found[i]=max>min && (i==4 || i==5 ? min>=0 : min<0 && max>0);
                    }
                    xboxAxes=true; for(boolean present:found) xboxAxes &= present;
                    System.out.println("XBOX_AXES expectedAndroidAxes="+Arrays.toString(axes)+" valid="+xboxAxes);
                }
            }
        }
        boolean valid = matchingPads == 1 && bridgePads == 1 && physicalPads==0 && dual && xboxAxes && xboxButtons;
        System.out.println("NS09_FRAMEWORK xboxJoystickPads="+matchingPads+" bridgePads="+bridgePads+" physicalNS09Pads="+physicalPads+" dualAmplitude="+dual+" xboxAxes="+xboxAxes+" xboxButtons="+xboxButtons+" valid="+valid);
        return valid ? 0 : 10;
    }
    public static void main(String[] args) {
        try {
            if(args.length>1) targetAddress=args[1];
            /* EventHub removal/addition is asynchronous. No vibration is generated. */
            int result = snapshot();
            if (args.length > 0 && args[0].equals("--require-bridge")) {
                for (int i=0; result != 0 && i<5; i++) { Thread.sleep(300); result=snapshot(); }
                System.exit(result);
            }
        } catch (Throwable error) {
            System.err.println("FrameworkProbeUnavailable: "+error);
            System.exit(11);
        }
    }
}
