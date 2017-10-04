
void usb_reader(void * pvParameters)
{
    std::string buffer = "";
    char temp[512] = { 0 };
    const char SEND[] = "telemetry ascii\n";
    const char GET[]  = "telemetry get ";
    const char SET[]  = "telemetry ";

    printf("LPC:");

    static const CLI_Command_Definition_t xStatsCommand =
    {
        "stats",
        "stats: Outputs some status information.\r\n",
        prvTaskStatsCommand,
        0
    };
    FreeRTOS_CLIRegisterCommand( &xStatsCommand );

    // Peripheral_Descriptor_t xConsole;
    // int8_t cRxedChar, cInputIndex = 0;
    // BaseType_t xMoreDataToFollow;
    // /* The input and output buffers are declared static to keep them off the stack. */
    // static int8_t pcOutputString[ MAX_OUTPUT_LENGTH ], pcInputString[ MAX_INPUT_LENGTH ];

    // /* This code assumes the peripheral being used as the console has already
    // been opened and configured, and is passed into the task as the task
    // parameter.  Cast the task parameter to the correct type. */
    // xConsole = ( Peripheral_Descriptor_t ) pvParameters;

    // FreeRTOS_read( xConsole, &cRxedChar, sizeof( cRxedChar ) );

    while(true)
    {
        char * fgets_success = fgets(temp, 512, stdin);
        if(fgets_success != NULL)
        {
            buffer += temp;
        }
        if(buffer.find("\n") != std::string::npos)
        {
            char * send_telemetry = strstr(buffer.c_str(), SEND);
            char *  set_telemetry = strstr(buffer.c_str(), SET);
            char *  get_telemetry = strstr(buffer.c_str(), GET);
            // printf("buffer ==%s==\n", buffer.c_str());
            if(send_telemetry != NULL)
            {
                printf("LPC: telemetry ascii\n");
                tlm_stream_all(stream_tlm, NULL, true);
                printf("\x03\x03\x04\x04   Finished in 0 us\n");
            }
            else
            if(get_telemetry != NULL)
            {

            }
            else
            if(set_telemetry != NULL)
            {
                printf("set_telemetry = %s\n", set_telemetry);
                char component_name[128] = { 0 };
                char variable_name [128] = { 0 };
                char variable_value[128] = { 0 };

                int found = sscanf(set_telemetry, "telemetry %s %s %s", component_name, variable_name, variable_value);

                if(found != 3)
                {

                }
                else
                if (tlm_variable_set_value(component_name, variable_name, variable_value))
                {
                    printf("%s:%s set to %s\n", component_name, variable_name, variable_value);
                }
                else
                {
                    printf("Failed to set %s:%s to %s\n", component_name, variable_name, variable_value);
                }
            }
            buffer = "";
        }
        vTaskDelay(100);
    }
}