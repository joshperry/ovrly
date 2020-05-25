$ProgressPreference = "SilentlyContinue"
$ErrorActionPreference="Stop"

# powershell core does not include Invoke-WebRequest
function Invoke-FastWebRequest
{
    [CmdletBinding()]
    Param(
    [Parameter(Mandatory=$True,ValueFromPipeline=$true,Position=0)]
    [System.Uri]$Uri,
    [Parameter(Position=1)]
    [string]$OutFile
    )
    PROCESS
    {
        if(!([System.Management.Automation.PSTypeName]'System.Net.Http.HttpClient').Type)
        {
            $assembly = [System.Reflection.Assembly]::LoadWithPartialName("System.Net.Http")
        }

        [Environment]::CurrentDirectory = (pwd).Path

        if(!$OutFile)
        {
            $OutFile = $Uri.PathAndQuery.Substring($Uri.PathAndQuery.LastIndexOf("/") + 1)
            if(!$OutFile)
            {
                throw "The ""OutFile"" parameter needs to be specified"
            }
        }

        $client = new-object System.Net.Http.HttpClient
        $task = $client.GetAsync($Uri)
        $task.wait()
        $response = $task.Result
        $status = $response.EnsureSuccessStatusCode()

        $outStream = New-Object IO.FileStream $OutFile, Create, Write, None

        try
        {
            $task = $response.Content.ReadAsStreamAsync()
            $task.Wait()
            $inStream = $task.Result

            $contentLength = $response.Content.Headers.ContentLength

            $totRead = 0
            $buffer = New-Object Byte[] 1MB
            while (($read = $inStream.Read($buffer, 0, $buffer.Length)) -gt 0)
            {
                $totRead += $read
                $outStream.Write($buffer, 0, $read);

                if($contentLength)
                {
                    $percComplete = $totRead * 100 / $contentLength
                    Write-Progress -Activity "Downloading: $Uri" -PercentComplete $percComplete
                }
            }
        }
        finally
        {
            $outStream.Close()
        }
    }
}

######
# Install project dependencies
######

echo "MATHFU_VERSION: $Env:MATHFU_VERSION"
echo "CEF_VERSION: $Env:CEF_VERSION"

Push-Location $Env:USERPROFILE

# Get mathfu
git clone --recursive https://github.com/joshperry/mathfu.git
Push-Location mathfu
git checkout "$Env:MATHFU_VERSION"
Pop-Location

setx MATHFU_ROOT "$Env:USERPROFILE\mathfu"

# Get cef
$CEF_VERSION_ENC=[uri]::EscapeDataString($Env:CEF_VERSION)
Invoke-FastWebRequest -URI "http://opensource.spotify.com/cefbuilds/$CEF_VERSION_ENC.tar.bz2" -OutFile "$Env:USERPROFILE\$Env:CEF_VERSION.tar.bz2"
bunzip2 -d "$Env:CEF_VERSION.tar.bz2"
tar xf "$Env:CEF_VERSION.tar"
Remove-Item "$Env:CEF_VERSION.tar" -Confirm:$false

setx CEF_ROOT "$Env:USERPROFILE\$Env:CEF_VERSION"

Pop-Location
