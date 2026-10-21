function server = start_xapp_piradio_bridge(sdr1, port)

    if nargin < 2
        port = 5000;
    end

    fprintf("\n============================================\n");
    fprintf("Starting xApp <-> MATLAB <-> Pi-Radio bridge\n");
    fprintf("============================================\n");

    server = tcpserver("0.0.0.0", port, ...
                       "Timeout", 30);

    configureTerminator(server, "LF");

    configureCallback(server, ...
                      "terminator", ...
                      @(src,event) onCommand(src,event,sdr1));

    fprintf("Listening for xApp on TCP port %d\n", port);

end


function onCommand(src, ~, sdr1)

    try

        raw = readline(src);

        fprintf("\nReceived from xApp:\n%s\n", raw);

        msg = jsondecode(char(raw));

        cmd = upper(string(msg.cmd));


        % =========================================================
        % PING
        % =========================================================

        if cmd == "PING"

            response.status = "OK";
            response.message = "MATLAB/Pi-Radio bridge alive";

            writeline(src, jsonencode(response));

            return;
        end


        % =========================================================
        % SET BEAM
        %
        % Example:
        %
        % {"cmd":"SET_BEAM","beam_deg":20}
        %
        % =========================================================

        if cmd == "SET_BEAM"

            beam = double(msg.beam_deg);

            fprintf("Requested beam = %.2f deg\n", beam);

            tic;

            sdr1.calc_w(beam);

            status = sdr1.configure_realtime(0);

            elapsed = toc;

            fprintf("Pi-Radio beam applied.\n");
            fprintf("configure_realtime time = %.3f sec\n", elapsed);

            response.status = "OK";
            response.beam_deg = beam;
            response.piradio_status = status;
            response.elapsed_sec = elapsed;

            writeline(src, jsonencode(response));

            return;
        end


        % =========================================================
        % SET BEAM + NULL
        %
        % Example:
        %
        % {"cmd":"SET_BEAM_NULL",
        %  "beam_deg":20,
        %  "null_deg":-35}
        %
        % =========================================================

        if cmd == "SET_BEAM_NULL"

            beam = double(msg.beam_deg);
            nullAngle = double(msg.null_deg);

            fprintf("Requested beam = %.2f deg\n", beam);
            fprintf("Requested null = %.2f deg\n", nullAngle);

            tic;

            sdr1.calc_w(beam, nullAngle);

            status = sdr1.configure_realtime(0);

            elapsed = toc;

            fprintf("Beam + null applied to Pi-Radio.\n");

            response.status = "OK";
            response.beam_deg = beam;
            response.null_deg = nullAngle;
            response.piradio_status = status;
            response.elapsed_sec = elapsed;

            writeline(src, jsonencode(response));

            return;
        end


        % =========================================================
        % UNKNOWN COMMAND
        % =========================================================

        response.status = "ERROR";
        response.message = "Unknown command";

        writeline(src, jsonencode(response));


    catch ME

        fprintf(2, "Bridge error: %s\n", ME.message);

        try

            response.status = "ERROR";
            response.message = ME.message;

            writeline(src, jsonencode(response));

        catch
        end
    end
end