package common.networklayer.ipv4;

simple L3RelayUnit
{
    parameters:
        @class(commons::L3RelayUnit);
    gates:
        input icmpIn;
        output icmpOut;
        input ipIn[];
        output ipOut[];
        input ifIn[];
        output ifOut[];
        input ifOobIn[];
        output ifOobOut[];
}